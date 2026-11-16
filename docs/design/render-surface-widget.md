# Design: render surfaces

> **Status: proposed, nothing implemented.** Written against master `4d00d3fd`
> (2026-11-16), with the unmerged `origin/effect-brushes` (tip `30b6727b`) and
> `origin/remote-screenshot` (tip `82dbdf86`) read as context. The decisions
> under *Decided* were settled with the owner and are recorded, not reopened;
> the rest is this note's proposal, and what it could not settle is under
> *Open questions for the owner*. Names are working names; the owner chooses
> the final ones. The API contract will be Doxygen in the headers.

## Purpose

Application code - typically a 3D engine using bqui as its UI toolkit -
wants arbitrary ase content inside the UI: placed by layout, clipped,
transformed, possibly rounded, and composited with everything around it.
This note designs a **texture brush**, a **frame hook**, and the
**`renderSurface`** widget built on them, for four uses: a 3D viewport in
the layout; a fullscreen 3D background under glass; several viewports of
one scene; one application-sized texture in several shapes.

## Decided

- **First producer: ase code in the same process and render context.** Raw
  GL in a share-group context, Vulkan and platform buffers are future
  stages. **Pixels are SDR**; the engine tone-maps. **Time** is the
  window's frame clock (`ase::Frame`), so remote pause/step freezes the 3D.
- **Rendered in the UI frame.** The producer draws as part of the window's
  frame and the result is composited in that same frame: no swapchain, no
  added latency, 3D and UI in sync. Images are texture-backed; when the UI
  redraws for its own reasons and nothing requests a frame, the last images
  are reused and the application is not called.
- **Two primitives.** A brush made from an application-created
  `ase::Texture`; and a frame hook in the widget hierarchy, tied to a shared
  handle, called once per window frame with a list of surfaces. A fixed-size
  image is the application's own texture, a hook with an empty list, and a
  texture brush; there is no toolkit render-image object.
- **`renderSurface(handle, ...)` is the layout-sized placement.** Every
  placement owns a texture sized by its layout size and the window's scaling
  factor, carries an application key, and is listed to its handle's hook.
  It has no callback of its own.
- **The handle requests frames**, from any thread, with a timed variant. A
  request makes every window holding a hook of that handle produce a frame.
  Scope is per window by construction: a handle used in two windows gets
  one call per window frame, each with that window's surfaces.
- **Stability** is aimed for only at the widget/modifier level. The render
  tree, avg and ase may change freely, with no seams added for them.

## Where the code is

"Effect branch" means `origin/effect-brushes`.

- **One frame path per window.** `WindowBridge::onFrame`
  (`src/bqui/src/windowbridge.cpp`) runs on the app loop thread: resize,
  `makeTransaction` if `needsUpdate_` (signals, layout, render-tree update;
  an observer sets the flag, so an input set on the loop thread before this
  step is in this frame), render tree into `drawing_` only while
  `animating_`, then always `Painter::clearWindow`, `paintToWindow`,
  `presentWindow` and `flush`. The effect branch keeps this shape. The
  return value, `nullopt` unless animating, is the delay
  `GenericWindow::frame` turns into `nextFrameTime`.
- **The painter submits once, late.** `clearWindow` and `paintToWindow` only
  record; `Painter::flush`, called from `presentWindow`, submits
  (`src/avg/src/painter.cpp`). **Submission is FIFO across `submit`
  calls**; `GlRenderState::submit` reorders only within one
  (`compareCommand`, `src/ase/src/gl/glrenderstate.cpp`).
- **Pacing is per window and speaks "not before"**
  (`PlatformBase::renderDirtyWindows`, frames `frameStep` apart).
  `ase::Window::requestFrame` is loop-thread only (it writes
  `GenericWindow::nextFrame_` unguarded); `PlatformBase::requestFrame` is
  the thread-safe part. `ase::Frame` is `{ time, dt }`.
- **One render context per app**, made inside `App::run`
  (`src/bqui/src/app.cpp`) and not exposed: resources are valid in every
  window, but the application gets the context only through bqui.
- **`Instance` carries per-widget lists up the tree.** Input areas and
  keyboard inputs (`widget/instance.h`) are concatenated in
  `modifier/addwidgets.h` and `widget/reduce.h`, moved by
  `Instance::transform`, and clipped by `modifier::clip`
  (`src/bqui/src/modifier/clip.cpp`), which drops keyboard inputs outside
  its obb. Per-placement state is made in a widget's build function, as
  `scrollView` makes its inputs (`src/bqui/src/widget/scrollview.cpp`).
- **`ase::Texture` is a shared handle with identity equality**
  (`src/ase/src/texture.cpp`). `GlTexture::destroy` dispatches deletion to
  the GL thread, so the last reference may drop on any thread (a
  framebuffer's destructor also waits for it). Textures never resize in
  place. Filtering is fixed at creation - linear, clamp-to-edge, no
  mipmaps (`src/ase/src/gl/gltexture.cpp`); ase has no sampler state.
- **No `Drawing` element draws a caller's texture**; the glyph atlas is the
  only textured path (`src/avg/src/rendering.cpp`). On the effect branch
  `Brush` is a `std::variant<Color, Effect>` with `isSameKind`. Clipping is
  rectangular; a shaped fill lowers a `shape::AnyShape` to an
  `avg::ShapeFunction`, as `modifier::surface` and `avg::SurfaceNode` do
  there. Fills are not antialiased; there is no opacity concept.
- **The effect branch renders every window frame offscreen** into a
  full-window `TargetImage` (`FORMAT_SRGBA` over
  `FORMAT_DEPTH24_STENCIL8`); backdrop effects sample snapshots at
  `BackdropScope`s, and stacked containers put later children over earlier
  ones. Its ase adds what a producer needs: `PipelineDesc`
  (format-checked), `makeShader`, `getClipSpaceAdjustment`,
  `FORMAT_RGBA16F`, scissors and barriers.

## The API, sketched

Names are the owner's to choose; the shape is what matters.

```cpp
bqui::RenderHandle<CameraId> scene;          // shared; requests frames

auto views = bqui::widget::uniformGrid(...,
        bqui::widget::renderSurface(scene, CameraId::top),
        bqui::widget::renderSurface(scene, CameraId::front,
            bqui::shape::rectangle(radius)));

auto ui = views | bqui::modifier::onRenderFrame(scene,
        [&engine](bqui::RenderFrame<CameraId>& frame)
        {
            frame.submit(engine.shadowPass(frame));     // shared pre-pass
            for (auto& s : frame.surfaces())
                if (s.isVisible())
                {
                    frame.submit(engine.render(s.key(), s.target(), frame));
                    s.present();
                }
        });

scene.requestFrame();                         // any thread
```

The fixed-size case, with no toolkit object:

```cpp
auto card = shape::rectangle(r).fill(brush.signal)   // a bq input
    | modifier::onRenderFrame(mirror, [&](bqui::RenderFrame<>& frame)
        {
            if (!texture)                    // first call: make it
            {
                texture = frame.context().makeTexture({512, 512},
                        ase::FORMAT_SRGBA);
                brush.handle.set(avg::Brush(*texture));  // this frame
            }
            frame.submit(engine.renderMirror(*texture));
        });
```

## The frame hook

**A modifier, `modifier::onRenderFrame(handle, callback)`**, since it adds
no geometry, named like `onClick` and `onDraw`. Its build function makes a
*hook* per placement of the modified widget and adds it to a new
`Instance` list, concatenated in `addwidgets.h` and `reduce.h`. A hook is
mounted wherever its widget is, visible or not; the root is the usual spot.

**The frame object** (`RenderFrame<Key>&`) hands the callback:

- **time**: the window's `ase::Frame` (`time`, `dt`);
- **the render context**, the app's only one, for programs, pipelines and
  the application's own textures, which survive windows and placements;
- **`submit(ase::CommandBuffer)`**, any number of times, straight to the
  main render queue. Submits are FIFO and the painter submits only after
  the callback returns, so everything submitted here completes before any
  UI draw samples it; barriers are needed only inside the application's own
  buffers. Shared pre-passes (shadow maps, skinning) are earlier submits
  that belong to no surface. A toolkit-owned shared buffer would gain
  nothing, since bqui never inspects it;
- **`surfaces()`**: this window's live placements of the handle, in layout
  order, possibly empty.

**Each surface** gives `target()` (a framebuffer over the placement's
`FORMAT_SRGBA` colour texture, plus depth if the widget asked, and the
formats its `PipelineDesc`s must declare); `pixelSize()`, `logicalSize()`
and `scalingFactor()` (extent = layout size times the window's scaling
factor, rounded; a transform's scale is excluded, so a scale-in animation
does not reallocate); `key()`; `reason()` - *requested* (the handle has a
due request), *created*, *resized* (texture remade), or none (listed while
the hook runs for another reason); `hasContent()`, false after *created*
and *resized*, when the texture is undefined memory; `isVisible()`, by
layout, inside the window and its clips; `present()` (*Reporting
results*); and a per-placement slot (*Open questions* 6).

**When it is called.** Once in a window frame in which its handle has a
due request, a visible placement of it has no content, or the hook was just
mounted (so it can make resources first). Needing content does not itself
schedule frames - creation and resize already come with a transaction's
frame - and a hover animation in the UI repaints without calling it.

**The key** is a template parameter, `RenderHandle<Key>` (an empty type by
default, for the brush-only case), so `key()` is typed; registry, cells and
placements are type-erased in `.cpp` files behind thin typed wrappers. A
small integer would push a lookup, an `std::any` a checked cast, into every
callback.

## Where the callback sits in the frame

It must run after signals and layout have settled, so placement sizes are
current and a resize never renders into a stale target, and before the
UI's draw commands are submitted. `WindowBridge::onFrame` becomes:

1. **Resize and transaction**, as today. Layout is settled, and the
   placement and hook lists on `widgetInstance_` are current.
2. **Reconcile placements.** Each placement's obb after clips and the
   window rectangle gives visibility, its size the logical size,
   `aseWindow.getScalingFactor()` the extent. A visible placement whose
   extent changed gets a new texture, depth and framebuffer (*resized*);
   what it shows is unchanged for now.
3. **Call the due hooks**, once each, with their handle's placements.
4. **Settle shown textures.** A placement presented with a new texture
   (first content, or after a resize) sets its brush input to it and drops
   the old one. If that, or an input the callback set, raised
   `needsUpdate_`, `makeTransaction` runs again, so this frame's tree shows
   the new texture.
5. **Draw, paint, present and flush**, as today, returning the earlier of
   the UI's delay and the earliest due hook.

Sizes are settled at step 2; what must change is that the bridge cannot
see placements and bqui widgets have no scaling factor. Hence the
`Instance` lists - placements with their obbs, moved by
`Instance::transform` and marked invisible (not dropped) by `clip` outside
its obb; hooks untouched by `clip` - with the bridge supplying the scaling
factor. Visibility is by layout, not by the drawn image, so a transition
still sliding a placement in counts it at its target position: coarser
than the earlier draft's walk over `drawing_`, but known before the draw,
which lets step 4 put a presented texture into this frame's drawing. The
second transaction runs only when a shown texture changes (creation, a
resize, a first present): two per frame during a live resize (*Open
questions* 9).

## Reporting results

bqui must know which surfaces got an image, to pick what each shows.

- **(i) The callback returns the updated surfaces**, a missing one meaning
  "not updated". Pure and easy to test. But the value names surfaces, which
  are references into the frame, and ties completion to the return: an
  async producer cannot finish later, and a later acquire/present target
  has nowhere to put the acquire.
- **(ii) An explicit per-surface call, `surface.present()`.** It maps onto a
  later swapchain mode (acquire in `target()`, release in `present()`) and
  onto producers finishing on another queue ("present after this fence").
  Forgetting it means "not updated", silently - the same risk as an
  element left out of (i).

Shared pre-pass work stays out of either. Today both only flip a flag, so
the choice is about later stages, which favour the explicit call.
**Recommendation: (ii)**, a late `present()` being a debug assertion for
now. Owner's call (*Open questions* 2).

## Surfaces that are not updated

A listed surface that is not presented keeps what it shows: its previous
content if it has some; after a resize, the old texture stretched over the
new rectangle until the new one is presented, then released; if never
presented, nothing - whatever is behind shows.

The owner allowed undefined contents here; defining them costs one flag,
a second texture held only during a resize, and step 4's transaction,
which a resize pays anyway. The drawing then never samples undefined
memory, so bqui need not clear on creation. **Recommendation: define it.**
Owner's call (*Open questions* 3).

## Which surfaces are listed

**All live placements of the handle in this window, visible or not,** with
their reasons. Listing only those needing a draw would hide the rest from
an engine that redraws every view on a scene change (the common request);
listing all costs an entry each. bqui never asks content of an invisible
placement. **A request means the handle**: the next call lists every
surface as *requested*; the application redraws what it chooses.

**No per-surface request.** Continuous animation of one view is
`handle.requestFrame()` plus the engine drawing only that view, since the
rest keep their content - one call per frame either way. A per-surface
request would only change one entry's `reason()` (*Open questions* 4).

## Hook lifetime

- **Registration.** A hook registers a *cell* with its handle's registry (a
  mutex and weak references): an atomic pending flag, an atomic due time
  and a *waker*, a small object the `WindowBridge` owns that holds the
  window behind a mutex and is cleared when the bridge dies. A cell holds
  no callback or GPU resource, so another thread may own the last reference.
- **A request from outside** locks the registry, marks every live cell (or
  lowers its due time), collects the distinct wakers, unlocks, and wakes
  each: one post to the run loop that calls `ase::Window::requestFrame`
  there if the window still exists. The bridge clears a cell's flag before
  calling the hook, so a request made during the call is served next frame.
- **A newly mounted hook is due**, so a request made before any window
  existed is not lost. **Unmounting** happens in a transaction on the loop
  thread, which destroys the callback and its captures.
- **A hook with no placements** gets an empty list: the texture-brush case.
  **Placements with no hook** in their window never show anything; a debug
  warning says so once.
- **Several hooks of one handle in one window** would be handed the same
  surfaces. **Recommendation:** a debug assertion; in release the first in
  layout order is called. Allowing it needs a rule for who presents what,
  and no use needs two (*Open questions* 5).
- The registry never keeps a hook, placement, window or run loop alive.

## The texture brush

`Brush` gains a third kind next to colour and effect: a **texture** - an
`ase::Texture`, the rectangle it is mapped onto in the element's space (or
none, for the fill's bounds), and an opaque flag.

- **Ownership and equality.** The brush holds the texture by value (already
  a shared handle) and compares by texture identity and mapping. New
  contents in the same texture leave the brush equal: the tree does not
  change and the next paint samples them. A new texture is a new brush
  value, reaching the tree through a signal. Two texture brushes are
  `isSameKind` when they share the texture, and lerp their mapping.
- **Transforms, clipping, shapes, effects** come from existing machinery:
  `operator*(Transform, Brush)` moves the mapping with the shape, clipping
  is `ClipElement`, any shape is the fill geometry (aliased like every
  fill), and a later backdrop scope's snapshot contains the fill.
- **Lowering.** The renderer tessellates the fill as for a colour, then
  computes each vertex's uv from its device position through the inverse of
  the mapping, so clipping and rotation are exact. A textured pipeline
  modelled on the text pipeline comes in two variants: opaque
  (depth-writing, sorts freely) and premultiplied-blended (z order). No
  batching with colour meshes; fills only, not pens or text.
- **Pixel contract.** Premultiplied alpha. A `FORMAT_SRGBA` texture is
  decoded on sampling and blends in linear light like all UI content; with
  `GL_FRAMEBUFFER_SRGB` on, a producer writes linear values and **must not
  gamma-encode itself**. An opaque brush ignores alpha. Row order is
  render-target order: render with clip-space +y up (times
  `getClipSpaceAdjustment()`) as for a window, and the brush maps the top
  row to the top; an image uploaded top row first needs a mirrored mapping.
- **Filtering** is what the texture was made with: linear, clamp-to-edge,
  no mipmaps, so strong minification aliases. An option waits for sampler
  state in ase.

## The renderSurface widget

`renderSurface(handle, key, options)`, and `renderSurface(handle, key,
shape, options)`. A placement is a rectangle of the widget's size (a
`RectNode`) filled with a texture brush over its shown texture, or in the
shaped form a `shape::AnyShape` lowered to a `ShapeFunction` and filled the
same way, as `modifier::surface` does; avg clips only to rectangles, so
this is how a viewport is rounded.

- **State.** The build function makes a *placement* per build, as
  `scrollView` makes its inputs: handle copy, key, options, shown and
  pending targets, extent, content flag, slot and brush input. It maps size
  and brush to the fill node and adds the placement, with its obb, to the
  `Instance` list. Made with the widget value instead, placements would
  share one texture - the rejected model.
- **Identity.** Size and transform changes make new nodes over the same
  placement, so the texture survives them; a rebuilt widget is a new
  placement, *created*. A removed placement dies with its widget, in a
  transaction; the last drawing keeps its texture alive until replaced.
- **Options**, fixed per widget: depth format (none, or
  `FORMAT_DEPTH24_STENCIL8`), opaque, later the colour format (the HDR
  hook). An engine with its own HDR targets writes only its tone-mapped
  pass into the target and asks for no depth.
- **Layout and input**: stretches (natural size zero); input is the usual
  modifiers, in local logical coordinates. Producer resources tied to the
  old size on a resize are the application's to remake, in the slot.

## Redraw without a tree change

When only texture contents change - a requested frame, normally - the brush
is equal, no input changes, no transaction runs and `animating_` stays
false. `WindowBridge::onFrame` still clears, paints the unchanged
`drawing_`, presents and flushes on every frame it runs, so **a requested
frame recomposites today**, at one repaint per frame and no signal work.
What must change is outside the paint path: a request from another thread
must reach the window (the waker), and the bridge must return a delay when
idle for a timed request.

## Timed requests, and what is left for later

**Timed requests.** A request may carry a frame-clock time before which it
is not served. Each cell keeps its earliest due time; the bridge returns
the earlier of the UI's delay and its hooks' due times, which
`GenericWindow::frame` already turns into `nextFrameTime`. Two small
changes: the bridge returns a delay even when the UI is idle, and the
window gets a `requestFrame` variant that lowers `nextFrame_` to a given
time rather than "now". A timed request freezes under pause.

**An after-frame hook** is a later addition, meaning "after `presentWindow`
and `flush` have submitted this frame" - not after GPU completion or
present. It is nearly free (one call after `flush`), but no use needs it
yet and that meaning is easily mistaken for completion.

**ase gaps for a real 3D producer** (depth compare, culling, stencil ops,
per-draw viewport, multisampling, mipmaps, sampler state, cube/array
textures, instancing) are ase PRs on their own track; a lit cube works
without them.

## The uses, walked through

**Viewport in a docked panel.** `renderSurface(scene, mainCamera,
shape::rectangle(r))` in a `scrollView`, `onRenderFrame(scene, ...)` at the
root. Scrolled out, `clip` marks it: listed invisible, never asked for
content. A UI animation over it repaints without calling the engine.
Undocked into another window, it is a new placement there, *created* -
provided that window also has a hook for `scene`.

**Fullscreen background with glass.** `content | background(renderSurface(
scene, mainCamera))`. On the effect branch `background` stacks, so a
frosted-glass card's snapshot is taken after the image is painted and
blurs the 3D. An animating engine requests every frame. The cost over a
bare engine is one full-window textured quad, the effects, and the present.

**Four viewports on one handle.** Four `renderSurface(scene, camera)` in a
grid, one hook. A worker thread's `scene.requestFrame()` wakes the window
once; its next frame calls the hook once with four *requested* surfaces,
and the engine renders shadows once and then the four views. Orbiting one
viewport: the engine requests every frame and presents only that view; the
others keep their content. Dragging a splitter lists two as *resized*.

**A fixed-size texture in several shapes.** The hook's first call makes a
512x512 `FORMAT_SRGBA` texture from `frame.context()` and sets a brush
input over it (second sketch); a card, a thumbnail and a circle fill with
that brush at their own sizes. A request calls the hook with an empty list;
the engine renders once and every shape samples the result. With hooks in
two windows both are called; the application skips a frame time it has
already rendered. A new size is a new texture and brush value.

## Testing

**Dummy backend (CI, every leg).** `DummyRenderQueue` records submissions
on the effect branch and the dummy platform runs the real frame loop, so
headless app tests with a pause token can assert: the callback's submits
precede the painter's in the same frame; call counts (per request, on
mount, on create and resize, none while only the UI animates, coalescing,
a request during the callback served next frame); the list's reasons,
keys, sizes and visibility, per window; the presentation rules (nothing
before the first present, the old texture stretched after a resize, a
presented texture shown in the same frame); lifetimes (requests racing a
closed window or an unmount, a dropped handle, the second-hook assertion);
time under the stepped clock and timed requests not served early. In avg:
uv under rotation, clipping and a rounded shape, the two pipelines,
equality and lerp of texture brushes.

**Real GL (manual, CI-dark).** sRGB round trip, row order, blending and
glass over 3D need readback in `bquiheadlesstest` or `window.screenshot`
(remote-screenshot branch): a producer clearing to a known colour must read
back as that exact sRGB value inside the viewport, the background outside
the rounded corner, and a blurred mix under glass.

## Dependencies on unmerged work

- **From the effect branch:** `PipelineDesc`, `makeShader`, barriers, the
  text pipeline as the textured pipeline's model, the always-offscreen
  target and stacked containers (without which glass cannot sample the 3D),
  and the `std::variant` `Brush` with `isSameKind`. Building on master
  would mean rebasing onto the pipeline API and painter the effect branch
  rewrites, and would miss the background case: **build on the effect
  branch once it lands.**
- **From remote-screenshot:** readback, for tests only.

## Staged plan

Each stage is one PR, after the effect branch is on master.

1. **avg: the texture brush.** The brush kind, equality and `isSameKind`,
   the textured pipeline (two variants), lowering with per-vertex uv.
   Dummy tests on command shape and vertex data.
2. **bqui: frame hook, handle and requests.** `RenderHandle<Key>`, registry,
   cells and window wakers, `modifier::onRenderFrame` and the hook list on
   `Instance`, the frame object with `submit` and an always-empty surface
   list, bridge step 3 and the transaction re-run of step 4. Headless tests
   for call counts and lifetimes. This alone serves the fixed-size case.
3. **bqui: `renderSurface`.** The placement list on `Instance` (merge
   points, `transform`, `clip`), bridge steps 2 and 4, targets and resize,
   the surface entries with `present()`, the shown-texture rules, options
   and the shape-taking form.
4. **Timed requests.** The `GenericWindow` change, the bridge's combined
   delay, the timed form on the handle; tests under a pause token.
5. **Demo and docs.** A 3D page in `testapp1` covering the four uses, the
   screenshot script, `src/bqui/readme.md`, the bqui and avg `AGENTS.md`,
   and a `docs/decisions.md` entry for the in-frame mode and primitives.

The ase 3D gaps proceed on their own and do not block stages 1-4.

## Future stages

- **(a) Rendered independently** (own rate and thread, the UI sampling the
  newest image): needs a separate context or queue and fences, where
  `present()` after a fence earns its keep.
- **(b) Into the window target:** saves the background's full-screen copy,
  but the engine's depth collides with avg's; only if profiling says so.
- **(c) Foreign producers and external memory:** share-group GL, Vulkan,
  platform buffers, with explicit synchronisation.
- **(d) UI inside a 3D scene**, **(e) whole-UI HDR** (the colour format
  becomes a widget option), and **(f) a per-placement callback** layered
  over a hook as a convenience, if a use wants one.

## Open questions for the owner

1. **Names.** `renderSurface` sits next to the effect branch's
   `modifier::surface` and `avg::SurfaceNode` (a filled card), which its
   shaped form even uses; and `modifier::frame` already exists (a border),
   so the hook must not be `frame`. Widget alternatives: `viewport` (reads
   well, but is a GL term for a rectangle), `renderTarget` (names the
   texture, not the widget), `renderView`. *Recommendation:*
   `widget::renderView`, `RenderHandle`, `modifier::onRenderFrame`,
   `RenderFrame`.
2. **Reporting results.** *Recommendation:* `surface.present()`.
3. **Never-presented contents.** *Recommendation:* defined - not drawn until
   first presented, the old texture stretched until a resized one is.
4. **Per-surface requests.** *Recommendation:* none.
5. **Several hooks of one handle in one window.** *Recommendation:* debug
   assertion, first in layout order wins.
6. **Per-placement producer resources** (an engine's HDR targets, one set
   per view). *Recommendation:* a slot (`std::shared_ptr<void>`, say)
   destroyed with the placement; no release callback.
7. **Release while invisible.** *Recommendation:* release the texture when
   a frame finds the placement invisible; scrolling back lists it as
   *created*, drawn in that same frame once presented. The slot is kept.
8. **Slack allocation during resize.** *Recommendation:* exact extents now;
   coarse buckets need a per-draw viewport in ase and a uv rectangle in the
   brush, so revisit with that gap if profiles show the cost.
9. **How a new texture reaches the tree.** A brush input and a second
   transaction (proposed), or a render-tree node that reads the placement's
   texture at draw time (no second transaction, but a node that is not a
   value). *Recommendation:* the input; measure live resize first.
10. **Opaque by default.** *Recommendation:* yes, per widget.
11. **Timed requests.** *Recommendation:* stage 4, on the handle.
12. **Driven by a signal value**: the key from a bq signal, requesting a
    frame when it changes. *Recommendation:* later, as an overload.

## Non-goals

- Independent rendering, foreign producers, external memory, UI-in-3D and
  HDR (*Future stages*); texture brushes on strokes and text.
- New input kinds, group opacity, shaped clipping of arbitrary widgets and
  antialiased edges: toolkit features of their own.
- The ase 3D gaps, and partial redraw: an in-frame image repaints the UI
  target each frame it updates, as every UI change does.
