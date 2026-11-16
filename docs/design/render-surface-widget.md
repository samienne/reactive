# Design: render views

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
transformed, animated, possibly rounded, and composited with everything
around it. This note designs an **image slot** and **image brush** in avg,
and a **frame hook** and the **`renderView`** widget in bqui, for four uses:
a 3D viewport in the layout; a fullscreen 3D background under glass,
drawn straight into the window's UI target when allowed; several viewports
of one scene; one application-sized texture in several shapes.

## Decided

- **First producer: ase code in the same process and render context.** Raw
  GL in a share-group context, Vulkan and platform buffers are future
  stages. **Pixels are SDR**; the engine tone-maps. **Time** is the
  window's frame clock (`ase::Frame`), so remote pause/step freezes the 3D.
- **Rendered in the UI frame.** The producer draws as part of the window's
  frame and the result is composited in that same frame: no swapchain, no
  added latency, 3D and UI in sync. When the UI redraws for its own reasons
  and nothing requests a frame, the last images are reused and the
  application is not called - except for a direct view (below).
- **Two primitives.** A brush over a texture; and a frame hook in the
  widget hierarchy, tied to a shared handle, called once per window frame
  with a list of views. A fixed-size image is the application's own
  texture, a hook with an empty list, and a brush; there is no toolkit
  render-image object.
- **`renderView(handle, ...)` is the layout-placed view.** Every placement
  shows its own image, carries an application key, and is listed to its
  handle's hook. It has no callback of its own.
- **The handle requests frames**, from any thread; a timed variant comes in
  stage 5. A request makes every window holding a hook of that handle
  produce a frame. Scope is per window: a handle used in two windows gets
  one call per window frame, each with that window's views.
- **avg sees only the texture.** Filling it before the `Drawing` is painted
  is bqui's concern: no callbacks, handles, windows or frame requests in avg.
- **The render tree's animations size the textures.** Layout gives only the
  target; the size and position drawn in a frame come from the render tree
  evaluated at that frame's time (transitions, size and transform
  animations, entering and leaving nodes).
- **Exact extent every frame, for the first version.** A view's extent is
  its exact animated size in that frame's drawing, so a size animation
  means a new image size every frame. A textured view the hook does not
  update in a frame shows its old image stretched to the new geometry.
- **Direct views.** A view whose options allow it, and which covers the
  whole window, is drawn by the engine straight into the window's UI
  target: no texture of its own, no textured quad. Allowing is opt-in;
  once allowed, detection is automatic each frame, with no special widget
  (*Direct views*). It is a stage of the design, after the textured path.
- **Stability** is aimed for only at the widget/modifier level. The render
  tree, avg and ase may change freely, with no seams added for them.
- **Naming.** "View", not "surface": the effect branch already has
  `modifier::surface` and `avg::SurfaceNode` (a filled card), and the widget
  is a view onto a scene. `modifier::frame` is a border, so the hook is
  `onRenderFrame`.

## Where the code is

"Effect branch" means `origin/effect-brushes`.

- **One frame path per window.** `WindowBridge::onFrame`
  (`src/bqui/src/windowbridge.cpp`) runs on the app loop thread: resize;
  `makeTransaction` if `needsUpdate_` (signals, layout, `RenderTree::update`,
  which sets `animating_`); then, **only while `animating_`**,
  `renderTree_.draw(DrawContext(&painter_), Obb(window size), timer)` into
  `drawing_`, `animating_` becoming the returned continue flag; then always
  `Painter::clearWindow`, `paintToWindow`, `presentWindow` and `flush`. An
  idle frame repaints the kept `drawing_`. The effect branch keeps this
  shape. The return value, zero while animating and `nullopt` otherwise,
  is the delay `GenericWindow::frame` (`src/ase/src/genericwindow.cpp`)
  turns into `nextFrame_`, **overwriting** any request made during the
  callback.
- **The draw is the frame's geometry.** `RenderTree::draw` is a pure
  function of the time (`src/avg/include/avg/rendertree/rendertree.h`). A
  `ShapeNode` draws at its `Animated<Obb>`'s size at that time, then
  transforms the result by the obb's transform
  (`src/avg/include/avg/rendertree/shapenode.h`); a `TransitionNode` keeps
  drawing a leaving child until its animation ends
  (`src/avg/src/rendertree/transitionnode.cpp`).
- **Clips erase.** `ClipNode` (`src/avg/src/rendertree/clipnode.cpp`) clips
  its child's drawing, and `Drawing::clip` drops every top-level element
  whose painted rect misses the clip (`filterElementsByRect`,
  `src/avg/src/drawing.cpp`). `widget::bin` clips and `scrollView` wraps
  its content in a bin (`src/bqui/src/widget/bin.cpp`,
  `src/bqui/src/widget/scrollview.cpp`), so a view scrolled out is
  **absent** from the drawing, not present and culled.
- **The painter submits late.** `clearWindow` and `paintToWindow` only
  record, lowering the drawing as they go; `Painter::flush`, called from
  `presentWindow`, submits the uniform uploads and then the commands
  (`src/avg/src/painter.cpp`), with no lowered drawing cached between
  frames. **Submission is FIFO across `submit` calls**;
  `GlRenderState::submit` reorders only within one (`compareCommand`,
  `src/ase/src/gl/glrenderstate.cpp`).
- **`Drawing` on the effect branch** (`src/avg/include/avg/drawing.h`) is a
  flat list of `ShapeElement`, `TextEntry`, `ClipElement`, `RegionFill`,
  `BackdropScope` and `EffectElement`; the last three nest `SubDrawing`s
  (an effect's in its `EffectInput`s). Transforms on shape elements are
  baked into the geometry (`transformElement`, `src/avg/src/drawing.cpp`);
  clips, scopes and effects keep theirs. Outlines are lifted to the
  outermost drawing by every combining operation (`liftSubDrawing`). The
  renderer walks the elements with a `toRoot` transform and a clip rect,
  culling by `getElementRect(...).overlaps(rect)` and asserting unrotated
  clips (`generateMeshesForElements`, `src/avg/src/rendering.cpp`). Mesh
  vertices stay in element space under the mesh's own transform
  (`src/avg/include/avg/softmesh.h`), which never touches the brush.
- **`Brush`** is a `std::variant<Color, Effect>` with `isSameKind`, and
  `operator*(Transform, Brush)` applies only the transform's scale
  (`src/avg/include/avg/brush.h`). Much code reads "not an effect" as "a
  colour" (stage 1 lists the sites). No element draws a caller's texture;
  the glyph atlas is the only textured path.
- **Elsewhere in bqui and ase.** `ase::Window::requestFrame` is loop-thread
  only (it writes `GenericWindow::nextFrame_` unguarded), and on dummy and
  offscreen windows it only records the request
  (`src/ase/src/dummy/dummywindow.cpp`, `src/ase/src/offscreenwindow.cpp`);
  the platform's `requestFrame` is thread-safe and the only call that
  schedules a tick (`src/ase/src/platformbase.cpp`). The bridge's signal
  waker sets `needsUpdate_` and requests a frame synchronously. One render
  context per app is made inside `App::run` and not exposed. `Instance` is
  rebuilt member by member (`src/bqui/src/widget/instance.cpp`), and
  `addWidgets` concatenates its lists
  (`src/bqui/include/bqui/modifier/addwidgets.h`).
- **ase resources.** `ase::Texture` is a shared handle with identity
  equality and `getSize`/`getFormat`; the last reference may drop on any
  thread. `makeTexture` does no GL work: storage is allocated at first use
  on the GL thread (`src/ase/src/gl/gltexture.cpp`). Textures never resize
  in place; filtering is fixed (linear, clamp, no mipmaps). A GL
  framebuffer's destructor waits for the GL queue, and `setColorTarget`
  mutates state the GL thread reads, so retargeting one an earlier submit
  still uses is a race (`src/ase/src/gl/glframebuffer.cpp`). `glViewport`
  is the bound framebuffer's full size. The colour formats are
  `FORMAT_SRGBA`, `FORMAT_R8` and `FORMAT_RGBA16F`
  (`src/ase/include/ase/format.h`).
- **The effect branch renders every frame offscreen**; its ase adds
  `PipelineDesc`, `makeShader`, `getClipSpaceAdjustment`, scissors and
  barriers. The painter's *UI target* is a `TargetImage` per window, remade
  on resize, cleared (opaque black, colour and depth) every frame and
  copied to the window by one quad (`src/avg/src/painter.cpp`). z descends
  in paint order under GL's default less-than test; opaque meshes draw
  first (`src/avg/src/rendering.cpp`). Snapshots copy the UI target; the
  root is an implicit scope. ase has no viewport state.
- **The window's drawing.** The bridge puts a theme rectangle under the
  root through `modifier::background` (`windowBackground`), which stacks,
  and a stacked container paints each later child through
  `Drawing::over`, inside a `BackdropScope`. So
  `content | background(renderView(...))` draws the theme rectangle, then a
  scope that starts with the view and holds `content` in a nested scope.

## The API, sketched

Names are the owner's to choose; the shape is what matters.

```cpp
bqui::RenderHandle<CameraId> scene;          // shared; requests frames

auto grid = bqui::widget::uniformGrid(...,
        bqui::widget::renderView(scene, CameraId::top),
        bqui::widget::renderView(scene, CameraId::front,
            bqui::shape::rectangle(radius)));

auto ui = grid | bqui::modifier::onRenderFrame(scene,
        [&engine](bqui::RenderFrame<CameraId>& frame)
        {
            frame.submit(engine.shadowPass(frame));     // shared pre-pass
            for (auto& v : frame.views())
            {
                ase::Texture image = engine.acquire(v.pixelSize());
                frame.submit(engine.render(v.key(), image, frame));
                v.present(image);        // or render into v.target()
            }
        });

scene.requestFrame();                         // any thread
```

The fixed-size case: the brush is constant, the slot is filled later.

```cpp
bqui::RenderHandle<> mirror;
auto slot = avg::ImageSlot::make();           // empty: draws nothing
auto card = shape::rectangle(r).fill(avg::Brush(slot))
    | modifier::onRenderFrame(mirror, [&engine, slot](bqui::RenderFrame<>& f)
        {
            if (!slot->getTexture())
                slot->set(f.context().makeTexture({512, 512},
                        ase::FORMAT_SRGBA));
            f.submit(engine.renderMirror(*slot->getTexture()));
        });
```

## The image slot

avg's whole notion of the external entity:

```cpp
class ImageSlot final                         // avg; loop thread only
{
public:
    static std::shared_ptr<ImageSlot> make(
            std::optional<ase::Texture> texture = std::nullopt);
    void set(ase::Texture texture);
    void clear();
    std::optional<ase::Texture> const& getTexture() const;
    void setOwner(std::weak_ptr<void> owner, void const* tag);
    std::weak_ptr<void> getOwner(void const* tag) const;
};
```

- **Contents.** The current texture, possibly none; an empty slot draws
  nothing. Setting or clearing changes no brush, element or tree: the next
  paint samples the new state. There is no content rectangle: with exact
  extents nothing in stage 1 renders into part of a texture. Grow-only or
  pooled oversize allocation would add one later (*Future stages*).
- **Owner link.** An opaque weak pointer and a tag, stored for whoever made
  the slot and never read by avg; `getOwner` returns it only for the
  matching tag, so bqui never misreads an application's slot. It carries no
  callback and keeps nothing alive.
- **Mutation is loop-thread only**: bqui between the draw and the paint, the
  application inside its hook.
- **One form.** A brush straight from a texture is
  `Brush(ImageSlot::make(texture))`; a second image kind would add another
  case at every brush site stage 1 lists, for no use.

The `Drawing` stays a value in its structure and geometry; only the pixels a
slot names change behind it. A newly presented texture therefore needs no
new brush value, no signal and no second transaction.

## The image brush

`Brush` gains a third kind next to colour and effect: an **image** - a
`std::shared_ptr<ImageSlot>`, a mapping `Obb` (the rectangle, in the
element's space, onto which the whole texture is mapped), and an opaque
flag.

- **Mapping.** A brush made without one takes the bounds of the shape it
  fills when the `ShapeElement` is made, at the node's local size, before
  its transform. Elements are made by `Shape::fill`
  (`src/bqui/src/shape/shape.cpp`) and directly
  (`src/avg/src/rendertree/surfacenode.cpp`,
  `src/avg/src/builtineffects.cpp`), so one helper resolves an unset
  mapping on every path; `renderView` sets its own, and lowering asserts in
  debug that an image brush has one. `operator*(Transform, Brush)` and the
  float scale of `Drawing::operator*` compose into the mapping instead of
  scaling the brush, so it keeps local size apart from accumulated scale,
  which shape geometry, with the transform baked in, loses.
- **Equality.** Same slot (pointer identity), mapping and flag. Swapping or
  filling the slot never changes the brush, so the tree does not change.
  Two image brushes are `isSameKind` when they share the slot, and lerp
  their mapping.
- **Transforms, clipping, shapes, effects** come from existing machinery:
  clipping is `ClipElement`, any shape is the fill geometry (aliased like
  every fill), and a later backdrop scope's snapshot contains the fill.
- **Lowering.** The renderer reads the slot while lowering, tessellates the
  fill as for a colour, and computes each vertex's uv from its
  element-space position (before the mesh's transform) through the inverse
  of the element-space mapping, so clipping, rotation and a texture of
  another size than the mapping (stretching) are exact. Two image
  pipelines follow `makeTextPipeline` (`src/avg/src/painter.cpp`): the
  same target and depth formats, a sampler in the shader interface and the
  text vertex layout (colour, position, uv); one is opaque (depth-writing,
  sorts freely), one premultiplied-blended (z order). Effect sub-drawings
  draw with the same pipelines into scratch targets of the same formats.
  `ShapeElement` fills only: an image brush on a pen, text or `RegionFill`
  draws nothing and asserts in debug.
- **Opacity and outlines.** `isOpaque` (`src/bqui/src/shape/shape.cpp`)
  and `isOpaqueColour` (`src/avg/src/rendertree/surfacenode.cpp`) decide
  whether a fill adds an outline, which drives shadows. Proposed: an image
  fill follows its opaque flag, so an opaque view behaves like a solid
  fill (*Open questions* 7).
- **Pixel contract.** Premultiplied alpha and SDR: a presented texture is
  `FORMAT_SRGBA`, decoded on sampling and blended in linear light like all
  UI content; with `GL_FRAMEBUFFER_SRGB` on, a producer writes linear
  values and **must not gamma-encode itself**. An opaque brush ignores
  alpha. Row order is render-target order: render with clip-space +y up
  (times `getClipSpaceAdjustment()`) as for a window; an image uploaded top
  row first needs a mirrored mapping. Filtering is the texture's own, so
  strong minification aliases until ase has sampler state.

## Finding the slots in a drawing

An avg function lists every image use the paint will sample, with its
geometry for this frame:

```cpp
struct ImageUse
{
    ImageSlot* slot;     // valid only while the searched drawing is unchanged
    Obb mapping;         // window logical space: toRoot * element mapping
};
pmr::vector<ImageUse> findImageUses(pmr::memory_resource* memory,
        Drawing const& drawing, Vector2i pixelSize, float scalingFactor);
```

- **The walk** is the renderer's walk without tessellation: it descends
  `ClipElement`s, `BackdropScope`s and every `EffectInput` sub-drawing,
  composing `toRoot` and intersecting the clip rect exactly as
  `generateMeshesForElements` does, and records each `ShapeElement` filled
  with an image brush that passes the renderer's own cull, clipped to the
  window, in drawing order. "Found" therefore means "this frame's paint
  samples it", whichever step removed it otherwise: a clip that erased the
  element, the window cull, or a zero size. Inside an effect's input, a
  use is found when the effect element passes the cull. Factoring the
  shared walk out of `rendering.cpp` keeps the two from drifting. There is
  no occlusion: a view under an opaque card is still found.
- **Cost.** `Drawing` keeps a count of image uses, set by every
  constructor from elements, summed wherever outlines are merged
  (`operator+`, `liftSubDrawing`, `over`, `effect`, `scope`) and kept by
  operations that drop elements (`clip`, `filterByRect`, `withResource`):
  an upper bound, so a drawing with none returns at once. Otherwise the
  walk is O(elements) of rect and transform arithmetic. bqui calls it only
  for a new `drawing_` or scaling factor and reuses the list otherwise,
  which is exactly while its slot pointers stay valid.

## The frame hook

**A modifier, `modifier::onRenderFrame(handle, callback)`**, named like
`onClick` and `onDraw`. Its build function makes a *hook* per placement of
the modified widget and adds it to a new `Instance` list, carried by the
constructor, every setter and `makeInstance`, kept unchanged by
`Instance::transform` and concatenated by `addWidgets`;
`src/bqui/include/bqui/widget/reduce.h`, unused and already dropping the
introspection member, must carry it too or go. A hook is mounted wherever
its widget is; the root is the usual spot.

**The frame object** (`RenderFrame<Key>&`) hands the callback the window's
`ase::Frame` (`time`, `dt`); the render context, the app's only one, for
programs, pipelines, textures and slots; `submit(ase::CommandBuffer)`, any
number of times, straight to the main render queue; and `views()`, this
window's live views of the handle found in this frame's drawing, in
drawing order, possibly empty. Submits are FIFO and the painter submits
only after the callback returns, so everything submitted here completes
before any UI draw samples it; barriers are needed only inside the
application's own buffers. Shared pre-passes are earlier submits that
belong to no view.

**Each view** (`RenderView<Key>&`) gives `pixelSize()` (the exact extent:
the drawn local size - the layout size at this frame's animated obb -
times the window's scaling factor, rounded), `logicalSize()`,
`scalingFactor()`, `key()`, `reason()` - *requested* (the handle has a due
request), *created* (found for the first time), *revealed* (found again
after frames unlisted, its texture released), *resized*, *retargeted*
(entering or leaving the direct path), *repaint* (a direct view on a frame
painted for any other reason), or none (listed while the hook runs for
another reason) - the present call and target helper below, a read-only
`isDirect()`, and a per-view resource slot (*Open questions* 4).
`pixelSize()` excludes any transform scale - static, from a zoomed
container, or animated - so a scale-in keeps one size and a zoomed view
is stretched (*Open questions* 8). A view drawn more than once takes the
largest extent.

**When it is called.** Once in a window frame in which its handle has a
due request, a view of it is *created*, *revealed* or *resized*, or the
hook was just mounted (so it can make resources first). Created, revealed
and resized views come only from new drawings, which already ride a frame,
so a size animation calls the hook every frame; a hover animation
elsewhere in the UI repaints without calling it, unless a view of the
handle is direct: then every frame the window paints calls it.

**The key** is a template parameter, `RenderHandle<Key>` (an empty type by
default, for the brush-only case), so `key()` is typed; registry, cells and
views are type-erased in `.cpp` files behind thin typed wrappers.

## What the hook renders into

Two shapes for the target:

- **(A) Toolkit-owned.** bqui makes a colour texture, optional depth and a
  framebuffer at the extent; `view.target()` returns the framebuffer and
  `view.present()` swaps it into the slot. Simple for small users; format,
  size and texture lifetime stay inside the toolkit.
- **(B) Engine-provided.** The view states the extent, scaling factor and
  accepted format; the engine renders wherever it likes and calls
  `view.present(texture)`, which only sets the slot. An engine pools its own
  targets for size animations; its tone-mapping pass presents with no blit;
  it can render below the view's resolution and let the brush stretch it;
  the fixed-size case becomes the same mechanism; and it maps onto later
  swapchain and external-image producers. Against it, bqui must check the
  format and define a size mismatch (stretched), and the pixel contract
  becomes the engine's to keep.

(A) cannot grow into a later swapchain, whose images belong to the producer
(*Future stages*); (B) is that swapchain's present already.
**Recommendation: (B) as the primitive, plus a lazy `view.target()`** that
allocates a matching colour texture (and depth, if the widget asked) and
framebuffer on first use, after which `present()` without an argument
presents it. Simple users get (A) as a helper over (B); bqui keeps no
texture it was not asked for. Framebuffers are not cheap on GL (*Where the
code is*), so the helper keeps one framebuffer per view across frames and
recreates it, with its textures, only when the size changes; it never
retargets one an earlier submit may still use. A size animation therefore
recreates and stalls on the GL queue every frame: a known limit of
exact-extent sizing, which grow-only allocation (*Future stages*) lifts.
When the view is direct, `target()` returns the window's UI target instead
(*Direct views*); `present(texture)` is never direct.

**Accepted textures.** `present` takes a non-empty `FORMAT_SRGBA` texture,
the contract being SDR; `RGBA16F` waits for the HDR option. Proposed
default: anything else asserts in debug and clears the slot in release.

**Texture lifetime.** A presented texture stays alive while any slot in
the current drawing holds it, including a leaving node's after its view
died, and is released once no drawing references the slot. A pooling
engine may reuse a texture once its view has presented another (FIFO
submission orders the reuse after the frames that sampled it), but drops
a dead view's last texture rather than reuse it (*Open questions* 10).

**Reporting results.** `present()` is an explicit per-view call rather than
a returned list of updated views: a list names views, which are
references into the frame, and ties completion to the return, so an async
producer could not finish later. Forgetting `present()` means "not
updated", silently; a late one is a debug assertion for now (*Open
questions* 1).

## Where the application gets the render context

- **Today.** `App::run` makes the one `ase::RenderContext` as a local,
  `platform.makeRenderContext()` (`src/bqui/src/app.cpp`), and hands it to
  each `WindowBridge` and painter; nothing returns it. `RenderContext` is
  move-only over a shared impl. The application can supply the platform
  (`App::platform`) but not the context, so it has no context before the
  first frame or outside bqui's callbacks.
- **Transfer queue.** On GL a background context serves uploads and
  texture deletion when the platform enables it (`dispatchBg`), the main
  queue otherwise. Whether `RenderContext` calls are safe from a loader
  thread is unstated, so this note promises no loader-thread use.
- **`docs/design/render-architecture.md`** puts the context on each
  window; a frame object carrying its window's context fits that.
- **Stage 1** exposes it on the frame object only, enough for in-frame
  producers: a newly mounted hook is due, so its first call precedes the
  first paint. An engine that starts before the UI or owns the device
  needs more: **recommendation**, a later `App::renderContext(...)` beside
  `App::platform`, taking a context the application made from that
  platform (*Open questions* 14). Until then a context made apart from
  bqui's is a foreign producer.

## Where the hook sits in the frame

`WindowBridge::onFrame` becomes:

1. **Resize and transaction**, as today. The hook list is current.
2. **Draw the tree**, as today, while `animating_`: `drawing_` is this
   frame's geometry at this frame's time.
3. **Find image uses**, only for a new `drawing_` or a scaling factor that
   differs from the last search's (`ase::Window::getScalingFactor()`,
   compared each frame, since only a resize wakes the bridge today). Each
   use's slot is mapped to its live view through the owner link and bqui's
   tag; application slots and slots whose owner has expired (leaving
   views) are skipped. Views are grouped by handle; a view found for the
   first time is *created*, one found again after frames unlisted
   *revealed*, a changed extent *resized*. Views allowed to be direct are
   then qualified, every frame, from the cached list and window size.
4. **Call the due hooks**, once each, with their handle's views, from a
   copy of the hook list (strong references) so a callback cannot
   invalidate the iteration. A `present()` sets the view's slot at once.
   In a frame with a direct view, the painter first submits the UI
   target's clear on its own, so it precedes the hooks' submits.
5. **Settle**: clear the slots of views not found, releasing their
   textures and lazy targets (*Open questions* 5), and of direct views.
6. **Paint, present and flush**, as today, or, when a direct view's target
   was taken, as *Direct views* orders it. Lowering reads each slot now, so
   an image presented in step 4 is composited in this frame. The bridge
   returns the earlier of the UI's delay and the earliest due hook, and
   **zero when `needsUpdate_` is set or a cell is pending after the
   hooks**: a signal set or request made inside a hook asks for a frame
   during the frame callback, and `GenericWindow::frame` overwrites that
   with the returned delay, `nullopt` in an idle frame.

**There is no second transaction**: an input the callback sets is served
next frame through that zero delay. The bridge keeps weak references to
the last search's views, to release those that drop out. When only images
change, no transaction runs, the search's list is reused and step 6
repaints the kept `drawing_`: **a requested frame recomposites today**.

## Which views are listed

**Every live view of the handle found in this window's drawing**, with its
reason; a partially clipped view is listed. A view not found - scrolled
out, outside the window or zero-sized - is not listed that frame, and step
5 releases its texture but not its resource slot. Found again, it is
listed *revealed*; hooks run before the same frame's paint, so it is
presented before it is composited, with no blank frame. **A request means
the handle**: the next call lists every view as *requested*; the
application redraws what it chooses. Animating one view is
`handle.requestFrame()` plus the engine drawing only that view. A direct
view is also listed on every frame the window paints (*Direct views*).

A listed textured view that is not presented keeps what its slot holds: its
previous image, stretched over a changed extent; if never presented or
released while unlisted, nothing, and whatever is behind shows (*Open
questions* 2). The drawing never samples undefined memory.

## Entering and leaving views

- **Entering.** A new view appears in the drawing at its first animated
  size, possibly zero: no extent, not found. It is listed *created* at its
  first found non-empty extent and is presented in that frame, then
  *resized* each frame it grows.
- **Leaving.** When a widget is unmounted its view dies in that
  transaction, while the `TransitionNode` keeps drawing the node, and with
  it the brush, until the exit animation ends. The brush holds the slot and
  the slot its texture, so the node shows **its last image, frozen**,
  stretched by whatever the exit animates. The search skips it (the owner
  link has expired), so the hook never sees it and the engine can drop the
  view's resources at once, but not reuse the frozen texture. The slot and
  texture go when a drawing without the node replaces `drawing_`.
- **Assumption, verified in stage 3.** The view dying in the transaction
  rests on bq releasing a removed subtree's build-function state (view,
  hook captures) during that update, not at a later evaluation or through
  a `share()`d copy. A test checks that a leaving view is unlisted the
  frame it is removed; if not, the bridge marks views dead itself.

## Hook lifetime

- **Registration.** A hook registers a *cell* with its handle's registry (a
  mutex and weak references): an atomic pending flag, an atomic due time
  and a *waker*, a small object the `WindowBridge` owns that holds the
  window and the platform behind a mutex and is cleared when the bridge
  dies. A cell holds no callback or GPU resource, so another thread may own
  the last reference.
- **A request from outside** locks the registry, marks every live cell (or
  lowers its due time), collects the distinct wakers, unlocks, and wakes
  each: always one post to the run loop, never a synchronous call, whose
  task, if the window still exists, calls the window's `requestFrame` and
  then the platform's, which alone schedules a tick on dummy and offscreen
  windows. The bridge clears a cell's flag before calling the hook, so a
  request made during the call leaves it pending, served next frame
  through step 6's zero delay.
- **A newly mounted hook is due**, so a request made before any window
  existed is not lost. **Unmounting** happens in a transaction on the loop
  thread, which destroys the callback and its captures.
- **A hook with no views** gets an empty list: the image-brush case.
  **Views with no hook** of their handle in the window never show anything;
  a debug warning says so once. **Several hooks of one handle in one
  window** would be handed the same views: a debug assertion, the first in
  layout order called in release (*Open questions* 3).
- The registry never keeps a hook, view, window or run loop alive.

## The renderView widget

`renderView(handle, key, options)`, and `renderView(handle, key, shape,
options)`. A placement is a rectangle of the widget's size (a `RectNode`)
filled with an image brush over the view's slot, with an explicit mapping
of that size, or in the shaped form a `shape::AnyShape` lowered to a
`ShapeFunction` and filled the same way, as `modifier::surface` does; avg
clips only to rectangles, so this is how a viewport is rounded.

- **State.** The build function makes a *view* per build, as `scrollView`
  makes its inputs: handle copy, key, options, the slot (owner-linked back
  to the view with bqui's tag), extent and resource slot. The brush is
  constant for the view's life, so no brush input exists. Made with the
  widget value instead, views would share one image - the rejected model.
- **Identity.** Size and transform changes make new nodes with an equal
  brush over the same slot; a rebuilt widget is a new view, *created*. A
  removed view dies with its widget, in a transaction.
- **Options**: plain values fixed per widget - depth format for the
  target helper (none, or `FORMAT_DEPTH24_STENCIL8`), opaque (default on,
  *Open questions* 6), later the colour format (the HDR hook) - and one
  signal, `allowDirect` (default off, *Direct views*).
- **Layout and input**: stretches (natural size zero); input is the usual
  modifiers, in local logical coordinates.

## Direct views

A **direct view** is drawn by the engine straight into the window's UI
target, with no texture of its own and no quad sampling one.

**Opt-in, then automatic.** `allowDirect`, a per-placement `renderView`
option (decided), allows it; setting it is the application's consent to
the contract below. It only allows: an allowed view that does not qualify
in a frame is textured. Off, `target()` always hands out the view's own
framebuffer. `present(texture)` is never direct, whatever the option. On
and qualifying, `target()` hands out the UI target (its formats, so the
depth option does not apply) and the view's element is not painted; the
read-only `isDirect()`, known before the hook runs, tells the engine.
The option is a signal, as other bqui parameters are (`background`'s
brush, `frame`'s radius and colour). avg knows nothing of direct views,
so it skips the render tree: the view's state holds the current value,
updated when its instance evaluates, and qualification reads it each
frame. A change moves the view on the next frame by the transition rules
below. Its cost is one more input to the view's instance; the other
options stay plain values, fixed with the brush and target they shape.

**Qualifying**, after bridge step 3, from the search's walk, each use
also recording its walk position, its clip and whether it is inside an
effect input. An allowed view is direct when it is opaque and found once;
not inside an effect input (those paint into scratch targets); clipped by
nothing inside the window; the unshaped form, whose fill is its mapping
rectangle (avg cannot tell a rectangular `Shape` from another, so bqui
answers from the view it built), with that mapping axis-aligned,
unmirrored and covering the window; of the UI target's pixel size; and
not followed by an effect reading a snapshot taken before it (below). No
tint or group opacity exists to check. If two views qualify, the later in
paint order is direct and the earlier, under it, stays textured.

**The prefix.** "Precedes" means earlier in the renderer's walk: depth
first, in element order, into clips and backdrop scopes but not effect
inputs. Content reaches the UI target in that order: z descends along it,
opaque meshes resolve by depth, blended ones follow in order, snapshots
and effects are fenced by barriers. So an opaque view covering the target
hides all that precedes it, and the painter skips it rather than
requiring it absent. "Top level only" cannot hold: the root's content is
always inside a backdrop scope (*Where the code is*), so enclosing scopes
are allowed, and the window's theme rectangle is the usual prefix. A
skipped element is still seen through a snapshot taken before the view
(the root scope's, or one entered before it): textured, it holds the
prefix; direct, the engine's pixels. Its readers are effects after the
view in such a scope; `Drawing::over` gives each later child its own
scope, so stacking never makes one, but a flat combination can, and it
disqualifies the view: direct pixels always equal textured ones.

**Frame order**, in a frame with a direct view:

1. Before the hooks, the painter records the UI target's colour and depth
   clear and submits it alone; today `clearWindow` only records, and
   `flush` submits after the paint. FIFO puts it ahead of the engine.
2. The hooks run; the engine's pipelines declare the UI target's formats.
3. If its target was taken, the painter records a depth-only clear:
   UI z lies in (0.5, 1) under a less-than test, and the engine's depth
   would hide UI wherever it is nearer. It then paints the drawing without
   the prefix and the view's element, presents and flushes.

Backdrop snapshots above the view are taken in step 3, after the engine,
so glass blurs the 3D as in the textured path; a `window.screenshot`
readback follows the frame's submits and sees it too. If the hook
presents instead, or takes neither, the frame paints textured after the
early clear. ase has no viewport state, so the engine's draws cover the
whole framebuffer: one reason the first version is fullscreen-only.

**The contract.** The UI target is cleared every frame and remade on
resize: a direct view has no old image to stretch or reuse. It is
therefore **listed on every frame the window paints**, as *repaint* when
no other reason applies, and must be drawn; "no update keeps the old
image" does not hold, and a hover re-renders the 3D. An engine for which
that costs too much leaves the option off or presents its own texture.
A target taken with nothing drawn shows the clear colour, opaque black,
the prefix still skipped. The UI target is a new framebuffer after a
resize, and a view entering or leaving the direct path switches between
it and its own, listed *retargeted*: engines take `target()` every frame,
never keep it, and never change its attachments. A direct view's slot is
cleared, so one leaving the direct path must be presented that frame,
and one leaving the tree shows nothing during its exit.

**What it saves**: one full-window colour texture and framebuffer, one
full-window textured draw, and painting the prefix. For an engine that
renders HDR and tone-maps, its tone-map pass writes into the UI target:
the saving is one SDR texture and one blit, no more. Its HDR targets
remain, and so does the painter's copy of the UI target to the window.

## Timed requests, and what is left for later

**Timed requests.** A request may carry a delay before which it is not
served, relative because another thread cannot read the loop-local frame
clock (and `stepFrame` keeps its own time): the waker's task makes it a
due time from the window's last frame time. Each cell keeps its earliest
due time; the bridge returns the earlier of the UI's delay and its hooks'
due times, which `GenericWindow::frame` already turns into `nextFrame_`.
Two small changes: the bridge returns a delay even when the UI is idle,
and the window gets a `requestFrame` variant that lowers `nextFrame_` to a
given time rather than "now". A timed request freezes under pause.

**An after-frame hook** waits for a use. **ase gaps for a real 3D
producer** (depth compare, culling, stencil ops, per-draw viewport,
multisampling, mipmaps, sampler state, instancing) are ase PRs of their
own.

## The uses, walked through

**Viewport in a docked panel.** `renderView(scene, mainCamera,
shape::rectangle(r))` in a `scrollView`, `onRenderFrame(scene, ...)` at the
root. Partly scrolled out, it stays listed; fully out, the bin's clip
erases it: unlisted, its texture released; scrolled back, it is listed
*revealed* and presented before that frame's paint. A UI animation over
it repaints without calling the engine. Undocked into another window, it
is a new view there, *created*.

**Fullscreen background with glass.** `content | background(renderView(
scene, mainCamera, options))` with `allowDirect` set. The view covers the
window, so it is direct: the engine renders into `target()`, the UI
target, its tone-map pass writing there; the theme rectangle is skipped,
and a frosted-glass card's snapshot, taken after the engine's commands,
blurs the 3D. Every UI repaint calls the hook. Without the option, or
with `present(texture)`, the output costs one full-window textured quad
and a hover repaints without calling the engine.

**Four viewports on one handle.** Four `renderView(scene, camera)` in a
grid, one hook. A worker thread's `scene.requestFrame()` wakes the window
once; the hook gets four *requested* views and renders shadows once, then
the four views. Orbiting one viewport presents only that view; the others
keep their images. Dragging a splitter lists two as *resized* each frame,
at sizes the engine takes from its pool. A view animating out of the grid
shows its frozen last image.

**A fixed-size texture in several shapes.** The hook's first call fills an
application slot with a 512x512 texture (second sketch); a card, a
thumbnail and a circle fill with brushes over that slot. The search
reports three uses of a slot bqui does not own and skips them: the size is
the application's choice. A request calls the hook with an empty list; the
engine renders once and every shape samples the result.

## Testing

**Dummy backend (CI, every leg).** `DummyRenderQueue` records submissions
(effect branch), reached through the frame object's context since `App`
does not expose it. Headless tests on the real frame loop, paused and
unpaused (only unpaused catches a waker that schedules no tick), assert:
the callback's submits precede the painter's; call counts (per request,
on mount, every frame of a size animation, none while only the UI
animates); a signal set or request made in the hook served next frame;
reasons and extents under a stepped size animation; listing under a
scroll (listed partly clipped, unlisted and released when out, *revealed*
and presented in the same frame when back); the presentation rules (empty
before the first present, an old image stretched, a leaving view frozen
and unlisted the frame it is removed, the lazy target kept at one size, a
wrong format rejected); lifetimes (requests racing a closed window or an
unmount). In avg: `findImageUses` through clips, scopes and effect inputs
with the renderer's culling, the early return, mapping composition, uv
under rotation and stretching, brush equality and the stage 1 brush
sites.

**Direct views (dummy).** Each failing condition - option off, a
translucent view, two uses, an effect input, a cutting clip, the shaped
form, an offset or mirrored mapping, a size mismatch, a later reader of an
earlier snapshot - keeps the textured path; an allowed
`background(renderView)` under the window background is direct. Recorded
commands: the clear submitted before the hook's buffers, a depth-only
clear after them, then no quad for the view and no theme rectangle; a
hover repaint lists the view *repaint*; a resize and toggling the
`allowDirect` signal list it *retargeted* with a new framebuffer; a
target taken with nothing submitted paints the clear colour; `present`
instead paints textured.

**Real GL (manual, CI-dark).** sRGB round trip, row order, blending and
glass over 3D need readback (`bquiheadlesstest` or the remote-screenshot
branch's `window.screenshot`): a known clear colour must read back exactly
inside the viewport, the background outside its rounded corner, and a
direct view under glass must read back as its textured form does.

## Dependencies on unmerged work

- **From the effect branch:** its pipeline API, painter and text
  pipeline, the always-offscreen target and stacked containers (for glass
  over 3D), the nested `Drawing` and the `std::variant` `Brush`: **build
  on the effect branch once it lands.**
- **From remote-screenshot:** readback, for tests only.

## Staged plan

Each stage is one PR, after the effect branch is on master.

1. **avg: slot, image brush and search.** `ImageSlot`, the brush kind with
   its mapping, equality, `isSameKind` and transform and float-scale
   composition, the mapping helper on every `ShapeElement` path, the two
   image pipelines, lowering with per-vertex uv, the use count on
   `Drawing`, and `findImageUses` over the shared walk. Every site that
   treats a non-effect brush as a colour gains the image case:
   `src/avg/src/brush.cpp` (`getColor`, ordering, printing, float scale),
   `hash_append` in `src/avg/include/avg/brush.h` (it would read an image
   as an effect), `src/avg/src/lerp.cpp`, `src/avg/src/painter.cpp`
   (pipeline choice), `src/avg/src/rendering.cpp` (vertices, text) and the
   opacity tests named above. Dummy tests on commands, vertices and uses.
2. **bqui: frame hook, handle and requests.** `RenderHandle<Key>`, registry,
   cells and window wakers, `modifier::onRenderFrame` and the hook list on
   `Instance`, the frame object with `submit` and an always-empty view
   list, bridge step 4 and the zero-delay return. Headless tests for call
   counts and lifetimes. With stage 1 this serves the fixed-size case.
3. **bqui: `renderView`.** Views with owner-linked slots, bridge steps 3
   and 5, the view entries with `present(texture)` and the format check,
   the lazy target, the shown-image rules, leaving views (verifying the
   assumption above), options and the shaped form.
4. **Direct views.** The search records walk position, enclosing clip and
   effect-input nesting; the painter submits a clear on its own, clears
   depth alone and paints with a prefix and an element skipped; bqui adds
   `allowDirect`, qualification, `isDirect`, the UI target from
   `target()`, the *repaint* and *retargeted* reasons and the frame order.
   Dummy tests above; the glass readback by hand.
5. **Timed requests.** The `GenericWindow` change, the bridge's combined
   delay and the timed form on the handle, tested under a pause token.
6. **Demo and docs.** A 3D page in `testapp1` for the four uses, the
   screenshot script, the bqui readme, the bqui and avg `AGENTS.md`, and
   a `docs/decisions.md` entry.

The ase 3D gaps proceed on their own and do not block stages 1-5.

## Future stages

- **Grow-only allocation**: a texture used as a capacity with a content
  rectangle in the slot, rendered into through a per-draw viewport (an ase
  addition next to the scissor), to cut per-frame reallocation and
  framebuffer stalls during size animations. ase would also want an
  asynchronous framebuffer destroy and a dispatched `setColorTarget`.
- **A swapchain, rendered independently** (own rate, thread or context).
  The slot is already the consumer half, a mailbox of the latest presented
  image; a swapchain is a producer-side pool of images calling the same
  `present(texture)` from outside the callback. It would add a thread-safe
  present that wakes the window, a fence for another context's images, a
  release notification so the producer may reuse an image, the desired
  extent as an event (one frame late, covered by stretch), and imported
  textures. Stage 1 therefore keeps `present` a slot operation taking a
  texture, reads each slot once per paint, and does not tie a view's
  image to a toolkit-owned target - the reason (A) is only a helper.
- **Partial direct views:** a view that does not cover the window drawn
  into the UI target through a per-draw viewport, with a scissored depth
  clear and an occlusion test in place of the skipped prefix.
- **Foreign producers and external memory:** share-group GL, Vulkan,
  platform buffers, with explicit synchronisation.
- **UI inside a 3D scene**, **whole-UI HDR** (the colour format becomes a
  widget option), and **a per-view callback** layered over a hook.

## Open questions for the owner

None of these is decided; each carries this note's recommendation.

1. **Reporting results.** *Recommendation:* `view.present()`.
2. **Never-presented contents.** An old image stretching until replaced is
   decided; what an empty slot shows is not. *Recommendation:* nothing,
   whatever is behind shows.
3. **Several hooks of one handle in one window.** *Recommendation:* debug
   assertion, first in layout order wins.
4. **Per-view producer resources** (an engine's HDR targets, one set per
   view). *Recommendation:* a slot (`std::shared_ptr<void>`, say) destroyed
   with the view; no release callback.
5. **Release when unlisted.** *Recommendation:* clear the slot when a
   search does not find the view, as the design above assumes, and keep
   the resource slot.
6. **Opaque by default.** *Recommendation:* yes, per widget.
7. **Outlines and shadows of image fills** (undecided). *Recommendation:*
   an image fill follows its opaque flag, opaque by default, so an opaque
   view adds an outline and casts a shadow like a solid fill.
8. **Transform scale in the pixel size** (undecided). Every transform
   scale is baked alike, so excluding animated scale excludes all of it
   and a zoomed view is stretched; including it means a new size every
   frame of a scale animation. *Recommendation:* for the first version,
   layout size times the scaling factor, ignoring transform scale.
9. **Driven by a signal value**: the key from a bq signal, requesting a
   frame when it changes. *Recommendation:* later, as an overload.
10. **The target's shape.** *Recommendation:* (B) `present(texture)` as
    the primitive, with a lazy toolkit `target()`.
11. **Slot to view.** An opaque tagged owner link on the slot (proposed), or
    a bqui map from slot to view. *Recommendation:* the link; a map needs a
    view list on `Instance` to fill it.
12. **Leaving views.** *Recommendation:* frozen last image, unlisted, the
    slot kept alive by the leaving node's brush.
13. **Application slots in the search.** *Recommendation:* skipped by bqui
    for now; a hook could later be told whether its slots are visible.
14. **Context outside the callback.** Frame object only, or also an
    application-made context handed to `App` (engine-first apps, start-up
    work). *Recommendation:* frame object in stage 1; `App::renderContext`
    as its own later stage, after loader-thread safety is checked.
15. **Skip the prefix or require the view first.** Requiring it first never
    qualifies a view under the window background. *Recommendation:* skip.
16. **Readers of an earlier snapshot** (an effect after the view in a scope
    entered before it; stacking never makes one). *Recommendation:*
    disqualify, so direct pixels always equal textured ones.
17. **An undrawn direct view** shows the clear colour, opaque black, not
    the theme. *Recommendation:* accept; the engine took the target.
18. **The early clear.** `flush` also ends the uniform ring's frame; the
    code does not settle whether two ring frames per window frame are
    safe. *Recommendation:* a painter entry that submits only the clear.

## Non-goals

- Independent rendering, foreign producers, external memory, UI-in-3D and
  HDR (*Future stages*); image brushes on strokes, text and region fills.
- New input kinds, group opacity, shaped clipping of arbitrary widgets,
  occlusion culling and antialiased edges: toolkit features of their own.
- The ase 3D gaps, and partial redraw: an in-frame image repaints the UI
  target each frame it updates, as every UI change does.
