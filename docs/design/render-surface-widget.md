# Design: a render surface widget

> **Status: proposed, nothing implemented.** Written against master `4d00d3fd`
> (2026-11-16), with the unmerged `origin/effect-brushes` (tip `30b6727b`) and
> `origin/remote-screenshot` (tip `82dbdf86`) read as context. The decisions
> under *Decided* were settled with the owner and are recorded, not reopened;
> everything else is this note's proposal, and the choices it could not
> settle are under *Open questions for the owner*. The API contract will be
> Doxygen in the headers once the PRs land.

## Purpose

Application code - typically a 3D engine that uses bqui as its UI toolkit -
wants to draw arbitrary ase content and have it appear inside the UI as an
ordinary widget: placed by layout, clipped, transformed, possibly rounded,
and composited with everything around it. This note designs that widget, the
*render surface*, and the avg and ase pieces under it.

Two uses shape stage 1:

- **A 3D viewport anywhere in the layout**, e.g. an editor's scene view in a
  docked panel: clipped by a scroll view, rounded corners, moved by a
  transform animation.
- **A fullscreen 3D background with the UI on top**, where glass and blur
  surfaces over it sample the 3D image like any other backdrop.

## Decided

- **First producer: ase code in the same process and the same render
  context.** Raw GL in a share-group context, Vulkan, and platform hardware
  buffers are future stages (*Future stages*).
- **Stage 1 renders in the UI frame.** The producer draws as part of the
  window's frame and the result is composited in that same frame: no
  swapchain, no added frame of latency, 3D and UI in sync. The surface is
  texture-backed; when the UI redraws for its own reasons and the surface
  has no pending request, the last image is reused and the producer is not
  called.
- **Frame API.** `requestFrame` schedules a frame and requests coalesce.
  `onFrame` is called once per window frame for which a frame was
  requested; it returns nothing, and calling `requestFrame` inside it
  guarantees a call on the next cycle. `onFrame` is also called without a
  request when the surface must be redrawn (resize, target recreated). A
  surface that is not visible gets no calls. `onFrame` receives the window's
  frame-clock time and step, not wall time, so remote pause/step freezes the
  3D too.
- **Pixels are SDR.** The engine tone-maps internally and hands over a
  standard-range image. Whole-UI HDR is a separate future feature.
- **Stability.** ABI is not a concern; API stability is aimed for only at
  the widget/modifier level. The render tree, avg and ase may change freely,
  and this design adds no seams or compatibility layers for them.

## Where the code is

Findings that shaped the design. "Effect branch" means `origin/effect-brushes`.

- **There is one frame path per window.** `WindowBridge::onFrame`
  (`src/bqui/src/windowbridge.cpp`) runs on the app loop thread: it applies a
  pending resize, runs the signal transaction, draws the render tree into
  `drawing_` only while `animating_`, then always calls
  `Painter::clearWindow`, `paintToWindow`, `presentWindow` and `flush`. An
  idle UI that is woken repaints the same `drawing_`. Its return value is the
  delay `GenericWindow::frame` turns into `nextFrameTime`
  (`src/ase/src/genericwindow.cpp`).
- **Frame pacing is per window and already speaks "not before".**
  `PlatformBase::renderDirtyWindows` renders a window whose `nextFrameTime`
  is due and that `canAcquire`s a slot; `earliestFrameTime` holds a due time
  to at least `lastFrameTime + frameStep` (`src/ase/src/platformbase.cpp`).
  `ase::Frame` is `{ time, dt }` (`include/ase/window.h`), the frame clock a
  pause token steps.
- **`ase::Window::requestFrame` is loop-thread only.** It writes
  `GenericWindow::nextFrame_` unguarded; `PlatformBase::requestFrame` is the
  thread-safe part (an atomic plus a `RunLoop::post`). `OffscreenWindow` and
  `DummyWindow::requestFrame` do not wake the platform at all, unlike the
  WGL and GLX windows.
- **One render context per app.** `App::run` makes one
  `ase::RenderContext` (`src/bqui/src/app.cpp`) and every window renders
  through it, so producer resources stay valid across windows.
- **Submission is FIFO across `submit` calls; reordering is within one.**
  `GlRenderState::submit` sorts draws for state batching
  (`src/ase/src/gl/glrenderstate.cpp`, `compareCommand`). On master it sorts
  each run of consecutive draws by framebuffer handle first; on the effect
  branch runs end at every non-draw command and at `pushBarrier()`, and a
  draw that samples a texture rendered earlier in the same buffer needs a
  barrier between them. Two separate submits are never interleaved.
- **The effect branch renders every window frame offscreen.** The painter
  draws into a full-window `TargetImage` (`FORMAT_SRGBA` over
  `FORMAT_DEPTH24_STENCIL8`) and presents it with one barrier plus a quad
  (`Painter::presentWindow`). Backdrop effects sample snapshots of that
  target taken at `BackdropScope`s; `Stacking::stacked` containers (used by
  bqui's `background`/`foreground`/`onDraw` on that branch) put later
  children in scopes over earlier ones.
- **The effect branch's ase is the one a producer needs.** It adds
  `PipelineDesc` (pipelines declare colour and depth formats, checked
  against the target by `Pipeline::matchesTarget` in debug builds),
  `ShaderInterface`/`makeShader` with a declared language,
  `getClipSpaceAdjustment`, `FORMAT_RGBA16F`, scissors and barriers. Master
  has none of these.
- **There is no way to draw a texture in a `Drawing`.** Elements are
  shapes, text, clips and region fills on master, plus `BackdropScope` and
  `EffectElement` on the effect branch, whose inputs are sub-drawings or the
  backdrop - never a caller's texture. `Brush` is a colour on master and
  `std::variant<Color, Effect>` on the effect branch (`brush.h`). The
  textured path that exists is the glyph atlas: the renderer's batch
  `Element` already carries a `std::vector<ase::Texture>`
  (`src/avg/src/rendering.cpp`), and `TextQuads` are textured quads.
- **Opacity is not a concept in bqui or avg.** Alpha lives in colours only.
  Shapes are not antialiased (no MSAA, no coverage AA on fills).
- **Input reaches widgets in local coordinates** (`InputArea` applies the
  inverse transform). Pointer moves are delivered only to areas holding a
  pressed button (`WindowBridge`'s `areas_`); hover is enter/leave without a
  position; there are no wheel events in ase; keys go to the focused input
  (`setFocusable`, `requestFocus`, `onKeyEvent`, `onTextEvent`).
- **The remote-screenshot branch adds readback.**
  `CommandBuffer::pushReadback(framebuffer, offset, size, cb)` and
  `Window::captureNextFrame` (`include/ase/window.h` there), used by
  `window.screenshot`.

## The frame, in order

A window frame with a visible surface runs, on the app loop thread, inside
`WindowBridge::onFrame`:

1. Resize and transaction, as today.
2. Draw the render tree into `drawing_` if animating, as today.
3. **Collect the visible image sources** of `drawing_` (*avg*, below): each
   surface that survives culling, with the logical size it is shown at.
4. **Serve the surfaces.** For each visible surface: make or remake its
   target if the pixel extent changed, and call `onFrame` if a request is
   pending or a redraw is required. The producer records into a fresh
   `ase::CommandBuffer`, which the bridge submits on the window's main render
   queue at once.
5. Paint, present and flush the UI, as today. The UI's draws sample the
   surface texture as an image brush.

Step 4's submit precedes the painter's `flush` submit, so on the render
thread the producer's commands complete before any UI draw that samples
their result. No barrier is needed between them; the producer needs
barriers only inside its own buffer, for passes that sample its own earlier
output. Everything the producer submits during `onFrame` to the main queue
directly (uploads, say) is ordered the same way.

## ase: what the producer is handed

`onFrame` receives a frame object. Proposed contents:

- **The render context** (`ase::RenderContext&`), for making programs,
  pipelines, buffers and its own textures and framebuffers. It is the app's
  one context, so resources made once survive resizes and window moves.
- **A command buffer** to record into, submitted by bqui when `onFrame`
  returns. A producer that submits to `getMainRenderQueue()` itself during
  the call gets the same ordering.
- **The target**: an `ase::Framebuffer` with the surface's colour texture
  attached, plus the depth attachment if the surface was created with one,
  and the two formats (`FORMAT_SRGBA`; the depth format or none) that the
  producer's `PipelineDesc`s must declare for draws into it.
- **Sizes**: the pixel extent, the logical size and the scaling factor.
- **Time**: the window's `ase::Frame` `time` and `dt`.
- **Why it is called**: requested, resized, or recreated (the target is new
  and holds only the clear; a producer that caches nothing ignores it).

**The surface owns the colour texture.** It is made and remade by bqui
against the app's context and is the only image the UI samples; the producer
never hands a texture over in stage 1. It is cleared once when created (to
transparent black, or opaque black for an opaque surface), so a producer
that skips a frame never exposes undefined memory; after that the producer
owns clearing.

**Depth is optional and offered by the surface.** A creation parameter picks
none or a depth format (`FORMAT_DEPTH24_STENCIL8` is what avg uses
elsewhere); with one the surface allocates a matching renderbuffer and
attaches it. An engine with its own pipeline - HDR in `FORMAT_RGBA16F`,
G-buffers, shadow maps - brings its own textures and framebuffers and writes
only its final tone-mapped pass into the target; it then asks for no depth.

**Resize and recreation.** Textures never resize in place (`TextureImpl` has
no setters; the effect branch's `TargetImage` and glyph atlas remake). The
surface remakes texture, depth and framebuffer when the pixel extent
changes, and calls `onFrame` with reason *resized* in that same frame, so a
stretched or stale image is never shown. Producer resources tied to the old
size (its own HDR targets) are the producer's to remake; the reason tells it
when. Device loss (`docs/design/render-architecture.md`, the recreation
reservations) would surface as *recreated*; GL never loses its context, so
nothing produces that reason yet.

**Pacing, and a "not before" time.** It fits the existing cadence directly.
`GenericWindow::frame` already turns the bridge's returned delay into
`nextFrameTime = frame.time + delay` in frame-clock time, and the platform
already schedules to the earliest due window. So a request may carry a
frame-clock time before which it is not served; the bridge returns the
earliest of the UI's own delay and the surfaces' due times, and serves a
surface only when `frame.time` has reached its due time. Two small changes
are needed: the bridge must return a delay even when the UI is idle (today
it returns `nullopt` unless animating), and the window needs a
`requestFrame` variant that lowers `nextFrame_` to a given time instead of
to "now", or an off-thread timed request would render immediately. Pacing
still holds a frame to at least `frameStep` after the last one. Being frame
clock, a timed request also freezes under pause.

**Frames in flight.** A surface texture is written in frame N and sampled
in frame N; frame N+1 rewrites the same texture. GL serialises this on its
one context. A backend with explicit hazards (Vulkan) would need the
surface to rotate images per frame in flight; under decision 5 that is the
backend PR's business, and nothing here is shaped for it.

**What ase lacks for a real 3D producer.** Independent of the widget, and
honest about stage 1's demo: pipeline state has blend, depth test and depth
write only - no depth compare function (GL's default `GL_LESS`), no face
culling, no stencil ops, no per-draw viewport (the framebuffer sets the
viewport to its full size on bind), no multisampling, no mipmaps, no sampler
state (every texture is linear, clamp-to-edge), no cube or array textures,
no instancing. A spinning lit cube works within this; a real engine needs at
least culling and depth compare. These are ase PRs on their own track, not
part of the widget stages; the doc records them so the demo's limits are
not mistaken for the widget's.

## avg: getting the image into a drawing

**An image brush.** `Brush` gains a third kind next to colour and effect: an
image, holding a shared reference to an *image source* (the surface's avg
face), the rectangle the image is mapped onto in the element's space, an
opacity multiplier and whether the image is opaque. A shape filled with it
shows the image mapped onto the rectangle, cut to the shape. This gives the
viewport use case everything through machinery that exists:

- **Placement and transforms** come from the element's transform, which
  `operator*(Transform, Brush)` already threads through; the mapping
  rectangle transforms with the shape, so rotation and scale need nothing.
- **Clipping** is the drawing's `ClipElement` like any fill.
- **Rounded or any shape** is the fill geometry:
  `rectangle(radius).fill(brush)`. Edges are aliased like every other fill
  (no AA in avg); that is avg's limitation, not this widget's.
- **Opacity** is the brush's multiplier, so a fade is an `Animated<Brush>`
  between two image brushes of one source; `isSameKind` holds for those and
  fails for any other pair, which takes the new brush at once. There is no
  group opacity in bqui and this does not add one.
- **Backdrop effects over it** need nothing: the image is painted into the
  window target in paint order, so a glass surface in a later scope samples
  a snapshot that contains it.

**Lowering.** The renderer tessellates the fill as it does a colour fill,
then computes a texture coordinate per vertex from its final device position
through the inverse of the brush's mapping, so clipping and rotation are
exact. It draws with a new textured pipeline modelled on the text pipeline
(position, uv, one sampler, the `MatrixBlock`), in two variants: opaque
(depth-writing, so it sorts freely in its run) and premultiplied-blended
(painted in z order). It is one draw per image fill; image fills never batch
with colour meshes, as effect brushes do not.

**The image source.** A small avg type held by `shared_ptr`: the current
colour texture (or none), whether the content is opaque, and a virtual hook
avg never calls itself. bqui's surface implements it. A source with no
texture draws nothing. Because the brush holds the source rather than a
texture, a drawing made before a resize samples the remade texture: the
render tree does not have to be redrawn when only the 3D changed, which is
what makes "UI idle, surface at 60 Hz" cost one repaint of an unchanged
`drawing_` per frame and no signal work.

**Visibility.** A free function over a `Drawing` and the target's pixel
rectangle returns each image source that would be drawn - after clips and
the same culling the renderer applies - with the largest logical size it is
mapped at. The bridge calls it in step 3. It is a separate walk rather than
a callback from inside `avg::render`, so the renderer never calls
application code and the producer's buffer can be submitted before the
painter's. A drawing is a flat list with nested scopes; the walk is cheap
next to the render it precedes.

**Colour.** The texture is `FORMAT_SRGBA`, sRGB-encoded, and holds
premultiplied alpha. With `GL_FRAMEBUFFER_SRGB` on (set by `GlRenderState`
for every GL context) a producer's shader writes linear values in [0, 1]
and the hardware encodes; sampling decodes, so the image blends in linear
light like all UI content. **The producer must not apply its own gamma
encode**, or the image is encoded twice. An opaque surface's alpha is
ignored. Row order is GL's: the producer renders with clip-space +y up
(times `getClipSpaceAdjustment()`, the identity on GL), exactly as it would
for a window, and the brush maps the texture's top row to the top of the
widget.

## bqui: the widget-level API

A sketch - names are proposals, the shape is what matters:

```cpp
bqui::RenderSurface surface(bqui::RenderSurfaceOptions()
        .depth(ase::FORMAT_DEPTH24_STENCIL8)
        .opaque(true));

surface.onFrame([&engine, surface](bqui::SurfaceFrame& frame) mutable
{
    engine.render(frame.getRenderContext(), frame.getCommands(),
            frame.getTarget(), frame.getPixelSize(), frame.getTime());

    if (engine.isAnimating())
        surface.requestFrame();
});

// A viewport: an ordinary widget, sized by layout.
auto viewport = bqui::renderSurface(surface)
    | bqui::modifier::onPointerDown(...)
    | bqui::modifier::setFocusable(true);

// Rounded, or any shape: the surface as a brush.
auto rounded = bqui::shape::rectangle(radius).fill(surface.brush());

// A fullscreen background under the UI.
auto ui = content | bqui::modifier::background(bqui::renderSurface(surface));
```

- **`RenderSurface`** is a handle (shared, cheap to copy), created by
  application code with no context, and alive as long as any copy or any
  drawing that references it. Options are fixed at creation: depth format,
  opaque, and later the colour format (the HDR hook). `onFrame` sets the
  producer callback; it runs on the app loop thread.
- **`requestFrame()`** is safe from any thread: it sets an atomic pending
  flag and, if the surface was visible in its last frame, posts to the app's
  run loop a task that calls `ase::Window::requestFrame` there (that call is
  loop-thread only). A timed variant takes a frame-clock time (above).
  Requests coalesce on the flag; the bridge clears it *before* calling
  `onFrame`, so a request made inside `onFrame` sets it again for the next
  cycle. A request while not visible stays pending without waking the
  window and is served in the first frame the surface is visible.
- **`renderSurface(surface)`** is a widget: a rectangle filled with
  `surface.brush()`, with a size hint that stretches (natural size zero - a
  viewport takes the space it is given). It reports no input areas; input
  is the usual modifiers on it. `surface.brush()` is a signal of the image
  brush mapped to the widget's own size, so `shape::...fill` works with no
  new plumbing.
- **Visibility** is what step 3 finds: in the drawing and not culled. A
  surface scrolled out of a scroll view, clipped away, or in a window that
  renders no frame gets no calls.
- **Texture extent** is the logical size it is shown at times the window's
  scaling factor, rounded to whole pixels. A transform's scale is not
  included: a viewport being animated in by a scale transform does not
  reallocate every frame, it samples (linearly) the extent it has.
- **During a resize** the surface is remade at the new extent and drawn in
  the same frame, because the layout that produced the new size and the
  `onFrame` call are in one frame. Nothing stale is ever shown, at the cost
  of one texture allocation per frame during a live window resize (see the
  open questions for slack allocation).
- **Input.** Everything a widget already has applies unchanged, in the
  widget's local logical coordinates: pointer down/up, move while a button
  is held (drag-to-orbit works), hover enter/leave, and keys plus text when
  focused. The producer converts to pixels with the scaling factor and
  flips y if it wants GL's convention. What a 3D viewport typically also
  wants and bqui lacks is **new and not stage 1**: pointer moves without a
  button (hover with position), wheel events (ase has none), relative
  pointer mode with a hidden, captured cursor, and key-state polling. Each
  is a toolkit feature in its own right, useful beyond surfaces.

## The two uses, walked through

**Viewport in a docked panel.** `renderSurface(surface)` inside a
`scrollView`, with a corner radius via `shape::rectangle(r).fill(...)`. The
scroll view's clip becomes a `ClipElement`; scrolled out, the visibility
walk drops it and the engine is not called; scrolled back with no request,
the surface is drawn from its texture if it still has one, or remade and
drawn with reason *recreated* if it was released. A UI animation over the
panel repaints the UI each frame without calling the engine.

**Fullscreen background with glass.** `content | background(renderSurface(
surface))`. On the effect branch `background` stacks, so the content sits
in a `BackdropScope` over the surface's image; a frosted-glass card's
snapshot is taken after the image is painted and blurs the 3D. The engine
requests every frame; the UI repaints every frame anyway because the
target is redrawn whole. The cost over a bare engine is one full-window
textured quad into the target, the effects, and the present quad.

## Testing

**On the dummy backend (CI, every leg).** `DummyRenderQueue` records
submissions on the effect branch (`recordSubmissions`), and the dummy
platform runs the real frame loop with fences completing inline, so a
headless app test with a pause token can assert:

- the producer's buffer is submitted before the painter's, in the same
  frame, and the UI draw binds the surface texture;
- `onFrame` counts: one per frame while requesting; none for an idle
  surface while the UI animates; one with no request after a resize; none
  while invisible (zero-size, clipped out, scrolled away); a request made
  while invisible served on becoming visible; a request inside `onFrame`
  served next frame; several requests between frames served once;
- `time`/`dt` equal the stepped frame clock; nothing is called while
  paused; a timed request is not served before its time and wakes the
  window when due;
- texture handle stable across same-size frames and replaced on resize
  (the same pattern `test/renderingtest.cpp` uses for the window target);
- avg unit tests for the image brush: uv correctness under rotation and
  clipping (vertex data), opaque versus blended pipeline choice, lerp
  between two image brushes, and the visibility walk against culling.

**Real GL (manual, CI-dark).** Pixel correctness - sRGB round trip, row
order, premultiplied blending, rounded edges, glass over 3D - needs a GL
context. `bquiheadlesstest` on the real backend with an offscreen window can
read back with `pushReadback` (remote-screenshot branch). The
remote `window.screenshot` gives an end-to-end check from a script: drive
the app client-side, step, capture, and compare a few pixels - a producer
that clears to a known colour must read back as that exact sRGB value
inside the viewport, the window background outside the rounded corner, and
a blurred mix under a glass card. A zero-time screenshot frame serves a
pending request with `dt` 0 and calls nothing otherwise, which is the
contract above, not a special case.

## Dependencies on unmerged work

- **From the effect branch, needed:** `PipelineDesc` and format-checked
  targets (the producer must declare the surface's formats), the shader
  interface and `makeShader` (how a producer makes programs at all on that
  ase), barriers and the run rules (inside the producer's buffer), the
  `std::variant` `Brush` and `isSameKind` (the image brush is a third
  alternative), the textured text pipeline as the model for the image
  pipeline, and the always-offscreen window target plus stacked
  containers, without which glass cannot sample the 3D background.
- **Could it go on master instead?** The viewport case could, by adding a
  colour-only brush variant and drawing straight to the window
  framebuffer. It would then be rebased onto a Brush, a pipeline API and a
  painter that the effect branch rewrites, and would miss the background
  use case. **Build on the effect branch once it lands.**
- **From remote-screenshot:** `pushReadback` and `window.screenshot`, for
  tests only. The feature does not depend on it.

## Staged plan

Each stage is one PR, after the effect branch is on master.

1. **avg: image brush and visibility.** The brush kind and image source
   type, the textured pipeline (two variants), lowering with per-vertex uv,
   lerp/`isSameKind`, the visibility walk. Dummy tests on command shape and
   vertex data.
2. **bqui: `RenderSurface` and `renderSurface`.** The handle, options,
   `onFrame`/`requestFrame` with the coalescing and thread rules, the
   bridge steps 3-4, target ownership and resize, the frame object. Headless
   app tests for every call-count rule above.
3. **Timed requests.** The `GenericWindow` change, the bridge's combined
   delay, the timed `requestFrame`; tests under a pause token.
4. **Demo and manual verification.** A 3D page in `testapp1` (a cube, a
   rounded viewport in a scroll view, a background with a glass card), the
   screenshot script, and the docs: `src/bqui/readme.md` concepts, both
   `AGENTS.md` files, a `docs/decisions.md` entry for the in-frame mode and
   the brush choice.

The ase gaps for 3D (culling, depth compare, viewport, MSAA, sampler state)
proceed on their own and are not blockers for stages 1-3; the demo uses what
exists.

## Future stages

**(a) Rendered independently.** The producer runs at its own rate and thread
and the UI samples the latest finished image from a mailbox: a small queue
of images (render into a free one, publish, the UI takes the newest, the
oldest unsampled is recycled). This decouples frame rates and lets a slow
engine not stall the UI. On the same ase context and GL queue it decouples
little: the work still serialises on one dispatcher, so a long 3D frame
still delays the UI's commands. It belongs with a separate context (a share
group on GL, a second queue elsewhere) and with fences between producer and
consumer, which is why it is not stage 1.

**(b) Rendering directly into the window target.** For an opaque fullscreen
background the image pass is a wasted full-screen copy; the producer could
draw straight into the painter's target before the UI. It collides with the
painter's use of depth: avg orders UI elements by z against a depth buffer
it clears per frame, so the engine's depth must be cleared (or the UI drawn
without depth testing) before the UI paints, and the producer must declare
the target's formats. Worth it only once profiling shows the copy matters.

**(c) Foreign producers and external memory.** Raw GL in a share-group
context, Vulkan, AHardwareBuffer/dmabuf/IOSurface/DXGI shared handles. The
image source's texture would then be imported rather than owned, with
explicit synchronisation (fences, keyed mutexes, semaphores) on both sides.
Mostly ase work: texture import per backend and cross-API sync primitives.

**(d) UI inside a 3D scene.** The reverse direction: bqui renders a window's
UI into a texture the engine samples on a mesh. The offscreen window
(`OffscreenWindow`, the effect branch's always-offscreen target) is most of
it; what is missing is handing the target texture to the engine and mapping
the engine's ray hits back to window coordinates for input injection.

**(e) Whole-UI HDR.** A photo editor wants the UI and images in extended
range end to end: an `RGBA16F` or scRGB window target, an HDR swapchain,
and a tone mapping or pass-through at present. For surfaces it means the
colour format becomes a creation option instead of fixed `FORMAT_SRGBA`;
the options type leaves room for exactly that and nothing more is done now.

## Open questions for the owner

1. **One surface in several places.** May a surface be shown by more than
   one widget at once (a mirror view, or two windows)? *Recommendation:*
   allow it within one window, rendering once at the largest extent and
   sampling it everywhere; forbid two windows (assert in debug) until
   independent mode, since two windows' frames are not one frame.
2. **Release while invisible.** Keep the texture while the surface is
   offscreen, or release it at once? *Recommendation:* release when a frame
   finds it invisible. The next visible frame remakes and redraws it in that
   same frame (reason *recreated*), so nothing stale shows, and memory stays
   bounded by what is on screen. The cost is one redraw when scrolled back.
3. **Slack allocation during resize.** Exact extents reallocate every frame
   of a live resize. Allocating in coarse buckets and rendering into a
   sub-rectangle avoids that but needs a per-draw viewport in ase and a uv
   rectangle in the brush. *Recommendation:* exact extents in stage 1;
   revisit with the viewport gap, if resize cost shows in profiles.
4. **Opaque by default.** *Recommendation:* yes. Engines produce opaque
   frames, an opaque draw sorts freely and needs no blending, and a
   transparent surface is an explicit option.
5. **Timed requests in stage 1.** *Recommendation:* include them as stage 3,
   as above - small, and the frame clock makes them correct under pause -
   but not block stages 1-2 on them.
6. **Who clears.** *Recommendation:* bqui clears only on creation; the
   producer clears every frame it draws. Clearing every frame in bqui too
   would cost a full clear per surface per frame for nothing.

## Non-goals

- Rendering independently of the UI frame, foreign producers, external
  memory, UI-in-3D and HDR (*Future stages*).
- New input kinds (wheel, hover moves, pointer lock), group opacity, and
  antialiased shape edges: toolkit features of their own.
- The ase 3D feature gaps listed under *ase*.
- Partial redraw: an in-frame surface repaints the UI target each frame it
  updates, as every UI change already does.
