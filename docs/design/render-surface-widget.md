# Design: a render surface widget

> **Status: proposed, nothing implemented.** Written against master `4d00d3fd`
> (2026-11-16), with the unmerged `origin/effect-brushes` (tip `30b6727b`) and
> `origin/remote-screenshot` (tip `82dbdf86`) read as context. The decisions
> under *Decided* were settled with the owner and are recorded, not reopened;
> the rest is this note's proposal, and what it could not settle is under
> *Open questions for the owner*. Names are working names; the owner chooses
> the final ones. The API contract will be Doxygen in the headers.

## Purpose

Application code - typically a 3D engine that uses bqui as its UI toolkit -
wants to draw arbitrary ase content and have it appear inside the UI as an
ordinary widget: placed by layout, clipped, transformed, possibly rounded,
and composited with everything around it. This note designs that widget,
`renderSurface`, the handle that asks it to redraw, a fixed-size *render
image* usable as a brush, and the avg and ase pieces under them. "Surface"
always means the layout-sized widget. Three uses shape the widget:

- **A 3D viewport anywhere in the layout**, e.g. an editor's scene view in a
  docked panel: clipped by a scroll view, rounded, moved by a transform
  animation.
- **A fullscreen 3D background with the UI on top**, where glass and blur
  over it sample the 3D image like any other backdrop.
- **Several viewports of one scene**, e.g. four cameras in a grid, all
  redrawn when the scene changes.

## Decided

- **First producer: ase code in the same process and render context.** Raw
  GL in a share-group context, Vulkan, and platform hardware buffers are
  future stages.
- **Rendered in the UI frame.** The producer draws as part of the window's
  frame and the result is composited in that same frame: no swapchain, no
  added latency, 3D and UI in sync. Each image is texture-backed; when the
  UI redraws for its own reasons and the image has no pending request, the
  last image is reused and the producer is not called.
- **The handle only requests frames.** `RenderSurfaceHandle` is a small
  shared object with `requestFrame()`, safe from any thread, and a timed
  variant. It owns no texture and no size.
- **Each placement owns its surface.** The widget is
  `widget::renderSurface(handle, onFrame)`. Every placement of it has its own
  texture, sized to that placement's size and scaling factor, created,
  resized and released with it. `onFrame` belongs to the widget, so
  placements sharing a handle can draw differently. One handle may be given
  to any number of `renderSurface` widgets, in one window or several; a
  request reaches every live placement and wakes each window holding one.
  The owner's reason: a shared surface gets into trouble when sizes change
  or when it sits at several points in the layout.
- **A fixed-size render image is a separate concept.** Made with a pixel size
  the application picks, it owns its texture and `onFrame`, takes requests
  of the same shape, and fills shapes as an image brush, so one image can
  be shown in any number of places. The widget is the main target.
- **Frame API.** Requests coalesce. `onFrame` is called once per window
  frame per placement with a pending request, and returns nothing. Inside
  it, `frame.requestFrame()` asks for the next cycle, so an animating view
  needs no captured handle; the handle is for requests from outside.
  `onFrame` is also called unrequested when the placement must be redrawn
  (created, resized, recreated), never while it is not visible, and
  receives the window's frame-clock time and step, so remote pause/step
  freezes the 3D too.
- **Pixels are SDR.** The engine tone-maps internally. Whole-UI HDR is a
  separate future feature.
- **Stability.** API stability is aimed for only at the widget/modifier
  level. The render tree, avg and ase may change freely; no seams or
  compatibility layers are added for them.

## Where the code is

"Effect branch" means `origin/effect-brushes`.

- **One frame path per window.** `WindowBridge::onFrame`
  (`src/bqui/src/windowbridge.cpp`) runs on the app loop thread: resize,
  signal transaction, render tree into `drawing_` only while `animating_`,
  then always `Painter::clearWindow`, `paintToWindow`, `presentWindow` and
  `flush`; an idle UI that is woken repaints the same `drawing_`. Its return
  value is the delay `GenericWindow::frame` turns into `nextFrameTime`
  (`src/ase/src/genericwindow.cpp`).
- **Pacing is per window and speaks "not before".**
  `PlatformBase::renderDirtyWindows` renders a window whose `nextFrameTime`
  is due and that `canAcquire`s; `earliestFrameTime` holds a due time to at
  least `lastFrameTime + frameStep` (`src/ase/src/platformbase.cpp`).
  `ase::Frame` is `{ time, dt }`, the clock a pause token steps.
- **`ase::Window::requestFrame` is loop-thread only.** It writes
  `GenericWindow::nextFrame_` unguarded; `PlatformBase::requestFrame` is the
  thread-safe part (an atomic plus a `RunLoop::post`). `OffscreenWindow` and
  `DummyWindow::requestFrame` do not wake the platform, unlike WGL and GLX.
- **One render context per app** (`App::run`, `src/bqui/src/app.cpp`), so
  producer resources are valid in every window.
- **Per-placement state is made in the build function.** A widget's build
  function runs once per placement (`makeWidget`, `widget/widget.h`);
  `scrollView` makes its inputs there (`src/bqui/src/widget/scrollview.cpp`),
  so two placements scroll independently, while `button` makes them with
  the widget value (`src/bqui/src/widget/button.cpp`), so all placements of
  one value share them. Rebuilding a widget destroys its state; `forEach`
  keeps survivors (`docs/design/arraysignal.md`).
- **Render-tree nodes are values,** cloned and carried from the old tree to
  the new by `update()`, matched by id and kind
  (`avg/rendertree/rendertreenode.h`). Size changes make new nodes, not new
  widgets.
- **Submission is FIFO across `submit` calls**; `GlRenderState::submit`
  reorders only within one (`compareCommand`,
  `src/ase/src/gl/glrenderstate.cpp`). On the effect branch a draw that
  samples a texture rendered earlier in the same buffer needs a
  `pushBarrier()` between them.
- **The effect branch renders every window frame offscreen** into a
  full-window `TargetImage` (`FORMAT_SRGBA` over
  `FORMAT_DEPTH24_STENCIL8`). Backdrop effects sample snapshots taken at
  `BackdropScope`s; stacked containers (bqui's `background`/`foreground`)
  put later children in scopes over earlier ones. Its ase adds what a
  producer needs: `PipelineDesc` (format-checked by
  `Pipeline::matchesTarget`), `ShaderInterface`/`makeShader`,
  `getClipSpaceAdjustment`, `FORMAT_RGBA16F`, scissors and barriers.
- **No `Drawing` element draws a caller's texture.** Elements are shapes,
  text, clips and region fills, plus `BackdropScope` and `EffectElement` on
  the effect branch, whose inputs are sub-drawings or the backdrop. The
  textured path that exists is the glyph atlas: the renderer's batch
  `Element` carries `std::vector<ase::Texture>` (`src/avg/src/rendering.cpp`).
- **Clipping is rectangular.** `Drawing::ClipElement` holds a `Rect`, and
  `modifier::clip` clips to the widget's obb (`src/bqui/src/modifier/clip.cpp`).
  A shaped fill takes a `shape::AnyShape` lowered to an `avg::ShapeFunction`,
  as the effect branch's `modifier::surface` and `avg::SurfaceNode` do.
- **Opacity is not a concept in bqui or avg**; alpha lives in colours. Fills
  are not antialiased.

## The frame, in order

A window frame with a visible placement runs, on the loop thread, inside
`WindowBridge::onFrame`:

1. Resize and transaction, as today.
2. Draw the render tree into `drawing_` if animating, as today.
3. **Collect the visible placements** of `drawing_` (*avg*, below), each
   with the logical size it is shown at.
4. **Serve them.** For each: record this window as its wake target, remake
   its target if the pixel extent changed, and call `onFrame` if a request
   is due or a redraw is required. The producer records into a fresh
   `ase::CommandBuffer`, which the bridge submits on the window's main
   render queue at once.
5. Paint, present and flush the UI, as today, sampling each placement's
   texture.

Step 4's submits precede the painter's `flush`, so the producers' commands
complete before any UI draw that samples them; no barrier is needed between
them. A producer needs barriers only inside its own buffer, and anything it
submits to the main queue itself during `onFrame` is ordered the same way.
Placements are served in drawing order; one `onFrame` may not assume
another's ran.

## ase: what the producer is handed

`onFrame` receives a frame object, `SurfaceFrame&`:

- **The render context**, for programs, pipelines, buffers and its own
  textures and framebuffers. It is the app's one context, so resources made
  once survive resizes, window moves and placements coming and going.
- **A command buffer**, submitted by bqui when `onFrame` returns.
- **The target**: a framebuffer with the placement's colour texture, plus a
  depth attachment if the widget asked for one, and the two formats its
  `PipelineDesc`s must declare.
- **Sizes** (pixel extent, logical size, scaling factor) and **time** (the
  window's `ase::Frame`).
- **Why it is called**: requested, created, resized or recreated.
- **`requestFrame()`** and its timed form: this placement again (*Open
  questions* 2).

**The placement owns its colour texture**, `FORMAT_SRGBA`, made by bqui. It
is cleared once when made (transparent, or opaque black for an opaque
placement), so a skipped frame never exposes undefined memory; after that
the producer clears. **Depth is a widget option**: none or a format
(`FORMAT_DEPTH24_STENCIL8` as avg uses), allocated as a renderbuffer. An
engine with its own pipeline (HDR in `FORMAT_RGBA16F`, G-buffers) brings its
own targets, writes only its tone-mapped pass into the target, and asks for
no depth.

**Resize and recreation.** Textures never resize in place (`TextureImpl` has
no setters). A placement remakes texture, depth and framebuffer when its
pixel extent changes and calls `onFrame` with *resized* in that same frame,
so nothing stretched or stale is shown. (GL serialises writing it in
frame N+1 after sampling in N; Vulkan would rotate images per frame in
flight - that backend's business.) Producer resources tied to the old
size are the producer's to remake, one set per placement (*Open questions*
3). Device loss (`docs/design/render-architecture.md`) would show up as
*recreated*; GL never loses its context, so nothing produces it yet.

**Pacing, and a "not before" time.** A request may carry a frame-clock time
before which it is not served. Each placement keeps its earliest due time;
the bridge returns the earliest of the UI's delay and its placements' due
times, which `GenericWindow::frame` already turns into `nextFrameTime`, and
serves a placement only once `frame.time` reaches its due time. Two small
changes: the bridge must return a delay even when the UI is idle (today
`nullopt` unless animating), and the window needs a `requestFrame` variant
that lowers `nextFrame_` to a given time rather than "now". Pacing still
holds frames `frameStep` apart, and a timed request freezes under pause.

**What ase lacks for a real 3D producer**, independent of the widget: depth
compare function, face culling, stencil ops, per-draw viewport,
multisampling, mipmaps, sampler state, cube/array textures, instancing. A
lit spinning cube works without them; a real engine needs at least culling
and depth compare. These are ase PRs on their own track, recorded so the
demo's limits are not mistaken for the widget's.

## avg: getting the image into a drawing

**An image brush, shared by both concepts.** `Brush` gains a third kind next
to colour and effect: an image, holding a shared reference to an *image
source* and the rectangle the image is mapped onto in the element's space,
or none to map it onto the fill's bounds. The widget is then an ordinary
shape node (`RectNode`, or `SurfaceNode` for the shape-taking form) filled
with an image brush over its placement, and a render image's
`brush()` is the same kind over its own source. A dedicated image node
would be marginally simpler for the widget alone, but it would be a second
mechanism next to the brush the render image needs anyway; one brush
kind serves both, and everything else comes from machinery that exists:

- **Transforms**: `operator*(Transform, Brush)` already threads the
  element's transform, and the mapping rectangle moves with the shape, so
  rotation and scale need nothing.
- **Clipping** is the drawing's `ClipElement`, like any fill.
- **Rounded or any shape** is the fill geometry. Edges are aliased like
  every fill; that is avg's limitation, not this widget's.
- **Animation**: two image brushes of one source are `isSameKind` and lerp
  their mapping; any other pair switches at once.
- **Backdrop effects over it** need nothing: the fill is painted into the
  window target in paint order, so a later scope's snapshot contains it;
  inside an `EffectInput`'s sub-drawing it is an effect input like any
  content.

Image brushes are for fills (`ShapeElement`, `RegionFill`); a pen or text
with one is not supported. There is no opacity multiplier: a fading
viewport waits for group opacity like any other widget.

**Lowering.** The renderer tessellates the fill as for a colour, then
computes each vertex's uv from its final device position through the
inverse of the brush's mapping, so clipping and rotation are exact. A new
textured pipeline modelled on the text pipeline (position, uv, one
sampler, `MatrixBlock`) comes in two variants: opaque (depth-writing, sorts
freely) and premultiplied-blended (z order). One draw per image fill; image
fills never batch with colour meshes, as effect brushes do not.

**The image source** is a small avg type held by `shared_ptr`: the current
texture (or none, drawing nothing) and the opaque flag. A widget placement
and a render image each are one. Because the brush holds the source
rather than a texture, a drawing made before a resize samples the remade
texture, so "UI idle, viewport at 60 Hz" costs one repaint of an unchanged
`drawing_` per frame and no signal work. Nodes are values: a size change or
a transform animation makes or interpolates nodes holding the same source.

**Visibility.** A free function over a `Drawing` and the target's pixel
rectangle returns each image source that would be drawn - after clips and
the renderer's culling, effect inputs included - with the largest logical
size it is mapped at (a placement can appear twice while a transition draws
old and new trees). It is a walk separate from `avg::render`, so the
renderer never calls application code and the producers' buffers are
submitted before the painter's.

**Colour.** The texture is sRGB-encoded with premultiplied alpha. With
`GL_FRAMEBUFFER_SRGB` on (`GlRenderState`) the producer's shader writes
linear values in [0, 1] and the hardware encodes; sampling decodes, so the
image blends in linear light like all UI content. **The producer must not
gamma-encode itself.** An opaque source's alpha is ignored. Row order is
GL's: the producer renders with clip-space +y up (times
`getClipSpaceAdjustment()`) as it would for a window, and the brush maps
the top row to the top of its rectangle.

## bqui: the widget-level API

A sketch - names are the owner's to choose, the shape is what matters:

```cpp
bqui::RenderSurfaceHandle scene;         // shared; only requests frames

auto view = [&engine, scene](Camera camera)
{
    return bqui::widget::renderSurface(scene,
            [&engine, camera](bqui::SurfaceFrame& frame)
            {
                engine.render(camera, frame);
                if (engine.isAnimating())
                    frame.requestFrame();      // this placement, next cycle
            },
            bqui::RenderSurfaceOptions().depth(ase::FORMAT_DEPTH24_STENCIL8));
};

scene.requestFrame();                    // any thread: redraw every placement

auto rounded = bqui::widget::renderSurface(scene,
        bqui::shape::rectangle(radius), onFrame, options);

auto ui = content | bqui::modifier::background(view(mainCamera));
```

- **`RenderSurfaceHandle`** is cheap to copy and shares a registry of
  placements. It needs no context and may exist before any window.
- **`renderSurface(handle, onFrame, options)`** is a widget: a rectangle in
  the widget's size filled with its placement's image brush, with a size
  hint that stretches (natural size zero).
  Options are fixed per widget: depth format, opaque, later the colour
  format (the HDR hook). It reports no input areas; input is the usual
  modifiers. The shape-taking overload fills a `shape::AnyShape` instead,
  lowered to a `ShapeFunction` as `modifier::surface` does; avg clips only
  to rectangles, so this is how a layout-sized viewport is rounded.
- **`onFrame`** runs on the loop thread. One widget value placed twice is
  two placements calling the same `onFrame`; views that draw differently
  (four cameras) are four widget values.
- **Visibility** is what step 3 finds. A placement scrolled or clipped away,
  or in a window that renders no frame, gets no calls.
- **Texture extent** is the logical size times the window's scaling factor,
  rounded. A transform's scale is not included, so a scale-in animation
  does not reallocate every frame. During a live resize the placement is
  remade and drawn in the same frame, at one allocation per frame
  (*Open questions* 5).
- **Input** is what every widget has, in local logical coordinates:
  press/release, move while held (drag-to-orbit works), hover enter/leave,
  keys and text when focused. Hover with position, wheel (ase has none),
  pointer lock and key-state polling are separate toolkit features.

## Placements: state, identity and registration

**Where the state lives.** The build function of `renderSurface` makes a
*placement* - one per build, the way `scrollView` makes its inputs - held
by `shared_ptr`. It is the image source and holds a handle copy, `onFrame`,
the options, texture, depth and framebuffer, the last extent, and a
*request cell*. The build maps the widget's size to a shape node filled
with an image brush over the placement, so the render tree, and after
drawing `drawing_`, hold it
too. Making it with the widget value instead, as `button` does its inputs,
would make every placement of a value share one texture - the model the
owner rejected.

**Identity and lifetime.** The placement is the identity. Size and
transform changes make new nodes over the same placement, so the texture
survives them. Rebuilding the widget - a parent signal yielding a new
widget rather than a `forEach` survivor - makes a new placement: a new
texture and an `onFrame` with *created*. On removal the placement dies when
the last tree and drawing holding it are replaced, on the loop thread, so
its GPU resources are released there.

**The registry** is a mutex and a list of weak references to request cells,
not to placements. A cell - an atomic pending flag, an atomic due time and
the wake target - holds no GPU resources, so a requesting thread that
briefly owns the last reference may destroy it; a weak reference to the
placement could release a texture off the loop thread. A placement
registers its cell when made and removes it in its destructor; requests
prune expired entries. The loop thread never holds the mutex during
`onFrame`.

**The wake target.** A cell has none until the bridge first serves its
placement, which sets it to that window's *waker*: a small shared object
the `WindowBridge` owns, holding the window behind a mutex the bridge clears
when destroyed. `wake()` posts once (an atomic coalesces) to the app's run
loop a task that calls `ase::Window::requestFrame` there if the window
still exists - the only safe way to reach it, as that call is loop-thread
only.

**A request from outside.** `handle.requestFrame()` locks the registry, sets
every live cell pending (or lowers its due time), collects the distinct
wakers, unlocks, and wakes each. A placement never shown, or not visible
now, is only marked and is served when it appears. The bridge clears a
cell's flag *before* calling `onFrame`, so a request during the call is
served next cycle.

**A request from inside.** `frame.requestFrame()` sets only this
placement's cell, on the loop thread; the bridge folds it into the delay it
returns, so nothing is posted.

**Who outlives whom.** A handle that outlives its windows finds cleared
wakers and expired cells: requests do nothing and clean up. Placements that
outlive the application's handle keep the registry alive through their own
copies; only outside requests stop. The registry never keeps a placement, a
window or the run loop alive.

## The render image

The second concept, separate from the widget: an image whose size the
application picks, shown wherever a brush can fill. It gives shaped fills
not tied to layout, and the identical image in many places rendered once
(a mirror, a minimap, a thumbnail of the main view, a texture on a card).

```cpp
bqui::RenderImage mirror(bqui::RenderImageOptions()
        .pixelSize({ 512, 512 }));         // chosen by the application
mirror.onFrame([&engine](bqui::SurfaceFrame& frame) { /* draw */ });
mirror.requestFrame();                     // any thread

auto a = bqui::shape::rectangle(radius).fill(mirror.brush());
```

- **Contract.** A shared object that owns one texture and one `onFrame`.
  Requests, the frame object, reasons, clearing, depth, opaque and colour
  are as for a placement; the frame reports the fixed pixel size with a
  scaling factor of 1.
- **Sizing.** Fixed pixels, no coupling to any window's scaling factor or
  to how large it is shown: each use scales it through its brush mapping.
  The application may change the size on the loop thread; it is remade and
  redrawn as *resized* at the next serve.
- **Filtering when scaled** is linear and clamp-to-edge today, with no
  mipmaps, since ase has no sampler state; minifying far below the pixel
  size aliases. The application picks a size near its largest use.
- **When `onFrame` runs.** *Recommendation:* once per request, in the first
  frame of any window in which some use of it is visible, with that
  window's time; other uses and windows sample the result (one context).
  Not visible anywhere, it stays pending and wakes nothing. The
  alternative - drawing whenever requested, visible or not - needs a frame
  outside any window, which the loop does not have.
- **Wake and lifetime.** It is its own request cell: a request wakes the
  windows that last drew it. Drawings and the application share its source,
  so its texture is released where the last reference goes; if that is not
  the loop thread, the release is posted to the loop. It is kept while
  invisible - the application chose its size so it can be reused.

Open for this concept: a filtering or mipmap option once ase has sampler
state (*recommendation:* add it there, not before), and whether a widget
placement should also expose a brush (*recommendation:* no - a view that
needs both a layout size and reuse renders into a render image the
application sizes from the layout).

## The uses, walked through

**Viewport in a docked panel.** `renderSurface(scene, shape::rectangle(r),
onFrame, options)` in a `scrollView`. Scrolled out, the walk drops it and
the engine is not called; scrolled back with no request, it is drawn from
its texture, or remade and drawn as *recreated* if released. A UI animation
over the panel repaints without calling the engine. Undocking the panel
into another window rebuilds it: a new placement, drawn as *created*.

**Fullscreen background with glass.** `content | background(renderSurface(
scene, onFrame, options))`. On the effect branch `background` stacks, so a
frosted-glass card's snapshot is taken after the image is painted and
blurs the 3D. The engine continues through `frame.requestFrame()`. The cost
over a bare engine is one full-window textured quad, the effects, and the
present quad.

**Four viewports on one handle.** Four `renderSurface(scene, ...)` widgets,
each capturing its own camera, in a grid; each has a texture of its own
cell size. An edit on a worker thread calls `scene.requestFrame()`: four
cells are marked, the window is woken once, and its next frame calls the
four `onFrame`s, each into its own target. Orbiting one viewport continues
through its `frame.requestFrame()` and redraws only that one. Dragging a
splitter gives *resized* only to the two it resizes. A viewport moved to a
second window keeps answering the same handle.

## Testing

**Dummy backend (CI, every leg).** `DummyRenderQueue` records submissions
on the effect branch, and the dummy platform runs the real frame loop, so a
headless app test with a pause token can assert:

- each producer's buffer is submitted before the painter's, in the same
  frame, and the UI draw binds that placement's texture;
- `onFrame` counts: one per frame while requesting; none for an idle
  placement while the UI animates; one unrequested on creation and resize;
  none while invisible; a request made while invisible served on becoming
  visible; one inside `onFrame` served next frame; several between frames
  served once;
- several placements: one `handle.requestFrame()` calls each once;
  `frame.requestFrame()` only its own; placements in two windows both
  woken; a rebuilt widget gets *created* and a new texture;
- lifetimes: requests from another thread after the window closed, after
  every placement went, or racing a removal do nothing; the application
  dropping its handle while placements live;
- `time`/`dt` follow the stepped clock; a timed request is not served
  early; the texture is stable across same-size frames and replaced on
  resize (as `test/renderingtest.cpp` checks the window target);
- avg: uv under rotation, clipping and a rounded shape (vertex data),
  opaque versus blended pipeline, lerp between image brushes, and the
  visibility walk against culling and effect inputs;
- a render image: served once per request with uses in two windows,
  not served while unseen, kept while invisible.

**Real GL (manual, CI-dark).** sRGB round trip, row order, blending,
rounded edges and glass over 3D need a GL context: readback in
`bquiheadlesstest`, or `window.screenshot` (remote-screenshot branch) from
a script - a producer clearing to a known colour must read back as that
exact sRGB value inside the viewport, the background outside the rounded
corner, and a blurred mix under glass.

## Dependencies on unmerged work

- **From the effect branch:** `PipelineDesc` and format-checked targets,
  `makeShader`, barriers, the text pipeline as the image pipeline's model,
  and the always-offscreen target plus stacked containers, without which
  glass cannot sample the 3D, and the `std::variant` `Brush` with
  `isSameKind` (the image brush is a third alternative).
- **On master instead?** The viewport case could draw straight to the
  window framebuffer, but would be rebased onto the pipeline API and
  painter the effect branch rewrites, and would miss the background case.
  **Build on the effect branch once it lands.**
- **From remote-screenshot:** readback, for tests only.

## Staged plan

Each stage is one PR, after the effect branch is on master.

1. **avg: image brush and visibility.** The brush kind and image source,
   the textured pipeline (two variants), lowering with per-vertex uv,
   lerp/`isSameKind`, the visibility walk. Dummy tests on
   command shape and vertex data.
2. **bqui: `RenderSurfaceHandle` and `renderSurface`.** The registry, request
   cells and window wakers, the per-build placement, options and the
   shape-taking form, bridge steps 3-4, target ownership and resize, the
   frame object with its own `requestFrame`. Headless tests for every
   call-count and lifetime rule above.
3. **Timed requests.** The `GenericWindow` change, the bridge's combined
   delay, timed forms on handle and frame; tests under a pause token.
4. **Demo and verification.** A 3D page in `testapp1` (a rounded viewport in
   a scroll view, four viewports on one handle, a background under a glass
   card), the screenshot script, and the docs: `src/bqui/readme.md`, the
   bqui and avg `AGENTS.md`, a `docs/decisions.md` entry for the in-frame
   mode, per-placement ownership and the shared brush.
5. **The render image.** `RenderImage`, its source, request cell, serving
   rule, `brush()`, resize; headless tests and a mirror on the demo page.

The ase 3D gaps proceed on their own and do not block stages 1-3.

## Future stages

- **(a) Rendered independently:** own rate and thread, the UI sampling the
  newest image from a mailbox. On one context and GL queue it decouples
  little; it belongs with a separate context or queue and fences.
- **(b) Into the window target:** saves the background's full-screen copy,
  but the engine's depth collides with avg's per-frame depth. Only if
  profiling shows the copy matters.
- **(c) Foreign producers and external memory:** share-group GL, Vulkan,
  platform buffers; an imported texture with explicit synchronisation.
- **(d) UI inside a 3D scene:** the offscreen window, plus handing over its
  texture and mapping ray hits back to window coordinates.
- **(e) Whole-UI HDR:** the colour format becomes a widget option; the
  options type leaves room for exactly that.

## Open questions for the owner

1. **Names.** `renderSurface`, `RenderSurfaceHandle`, `SurfaceFrame` and
   `RenderImage` are working names: "surface" is the layout-sized widget,
   "image" the application-sized brush source, and "frame" stays temporal
   (`onFrame`, `requestFrame`, the frame object). The effect branch already
   uses "surface" for a filled card (`modifier::surface`,
   `avg::SurfaceNode`), which the widget's own form fills with an image
   brush; the owner may want one of the two renamed.
2. **Scope of `frame.requestFrame()`.** *Recommendation:* this placement
   only - what a view animating itself (an orbit drag, a camera tween)
   means; `handle.requestFrame()` is the all-placements form and can still
   be called from inside `onFrame`.
3. **Per-placement producer resources.** An engine with its own HDR
   targets needs one set per placement and must know when one dies.
   *Recommendation:* the frame exposes a per-placement slot
   (`std::shared_ptr<void>`, say) that bqui destroys with the placement on
   the loop thread; no separate release callback.
4. **Release while invisible.** *Recommendation:* release the texture when
   a frame finds the placement invisible; memory now scales with
   placements. The next visible frame remakes and draws it in that frame
   (*recreated*), at one redraw when scrolled back. The slot in 3 is kept.
5. **Slack allocation during resize.** Exact extents reallocate every frame
   of a live resize, per resizing placement; coarse buckets need a per-draw
   viewport in ase and a uv rectangle in the brush. *Recommendation:*
   exact extents now; revisit with the viewport gap if profiles show it.
6. **Opaque by default.** *Recommendation:* yes, per widget: engines
   produce opaque frames, and an opaque draw sorts freely and needs no
   blending.
7. **Timed requests.** *Recommendation:* stage 3, on both handle and frame;
   small, correct under pause, and not blocking stages 1-2.
8. **Who clears.** *Recommendation:* bqui only on creation; the producer
   every frame it draws. Clearing in bqui too doubles a full clear per
   placement per frame.
9. **Driven by a signal value.** A further form could take a bq signal
   (`renderSurface(cameraSignal, onFrame)`), request a frame for that
   placement when the value changes, and pass the value to `onFrame` - a
   fit for views driven by UI state, riding the UI transaction.
   *Recommendation:* not in stage 1; the handle covers the engine-owned
   case, and the signal form can be added later as an overload over the
   same placement without changing the model.

## Non-goals

- Independent rendering, foreign producers, external memory, UI-in-3D and
  HDR (*Future stages*); image brushes on strokes and text.
- New input kinds, group opacity, shaped clipping of arbitrary widgets and
  antialiased edges: toolkit features of their own.
- The ase 3D gaps, and partial redraw: an in-frame image repaints the UI
  target each frame it updates, as every UI change does.
