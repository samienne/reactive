# Design: render views

> **Status: proposed, nothing implemented.** Written against the unmerged
> `origin/effect-brushes` (tip `30b6727b`, 2026-11-14), which every path
> below points at, and builds on `image-brush.md`. Items under *Decided*
> were settled with the owner and are recorded, not reopened. Items marked
> **proposed** are this note's working design, not confirmed by the owner;
> items marked **unverified** are claims the code has not been checked
> for. Names are working names; the owner chooses the final ones. The API
> contract will be Doxygen in the headers.

## Purpose

Application code - a 3D engine, a video player, an external renderer -
wants its pixels inside the UI: placed by layout, clipped, transformed,
animated, possibly rounded, and composited with everything around it,
glass included. This note designs the **render view**: a render-tree node
that shows "the current image" of a producer, two ways for a producer to
supply that image, and the bqui widget `renderView` over them.

The uses it has to serve: a 3D viewport in a docked panel; a fullscreen 3D
background under glass; several viewports of one scene; hardware video;
an engine with its own swapchain or queue.

## Decided

- **No slot.** A `Drawing` holds images only by value, in brushes
  (`image-brush.md`). The render tree's **view node** holds the current
  image between frames. A view shows the last image it was given; no new
  image leaves it unchanged; a view never given an image, or cleared, is
  skipped when the `Drawing` is built. The earlier slot model (a shared
  mutable `ImageSlot` inside the brush, found again by searching the
  `Drawing`) is dropped with everything that only existed for it.
- **Two layers, one mechanism.** The render tree only ever knows "this
  view shows this image".
  - **Layer 1, the primitive: an image source.** The producer owns its
    images and presents them to the view, from any thread.
  - **Layer 2, optional, on top: a frame callback.** The engine runs
    inside our frame, on our queue. The view owns the colour texture; the
    engine owns everything else. Layer 2 presents its texture through
    layer 1.
- **Reconfigure** tells a producer what the view needs: size now, size at
  rest, scale factor and visibility. It fires when the size at rest
  changes, not on every animation frame.
- **Carried over:** a shared **handle** whose frame callback is also the
  engine's tick; per-view `reasons()` flags, re-examined below; a handle's
  shared state is thread-safe, a single copy is not; reporting is a
  per-view **present**; time in layer 2 is the window's frame clock, so
  remote pause and step freeze the 3D; pixels are SDR and follow the pixel
  contract of `image-brush.md`; "view", not "surface", because the effect
  branch already has `modifier::surface` and `avg::SurfaceNode`.
- **One render context.** bqui's render views assume the App's single
  render context (`App::getRenderContext()` in `image-brush.md`). A queue
  per window and extra queues for engines are future mapping only.
- **Stability** is aimed for only at the widget and modifier level; the
  render tree, avg and ase may change freely.
- **Deferred, not blocked:** foreign and hardware images, an engine on its
  own ase queue, direct mode, and double buffering in layer 2. Each has a
  *Later* section saying what the first version does to keep it open.
- **Depends on the backdrop / effect-brushes stack** (PRs #172-#177 and
  #190, unmerged) and on `image-brush.md`, which lands first.

## Where the code is

Verified at `30b6727b` in a detached checkout of `origin/effect-brushes`.

- **The window frame.** `WindowBridge::onFrame`
  (`src/bqui/src/windowbridge.cpp:368-410`) runs on the loop thread:
  resize (`:375-381`, `Painter::setSize` with the scaling factor); the
  transaction when `needsUpdate_` is set (`:386-387`), which inside
  `makeTransaction` (`:251`) updates the render tree with
  `RenderTree::update` (`:333`, `src/avg/src/rendertree/rendertree.cpp:27`)
  and sets `animating_`; then, only while `animating_`, `RenderTree::draw`
  (`:389-399`, `rendertree.cpp:66`) into `drawing_`; then always
  `clearWindow`, `paintToWindow`, `presentWindow` and `flush`
  (`:401-404`, `src/avg/src/painter.cpp:321`, `:337`, `:347`, `:396`).
  An idle frame repaints the kept `drawing_`. It returns zero while
  animating and `nullopt` otherwise (`:408`).
- **A tree walk with geometry already exists:** `RenderTree::snapshot`
  (`rendertree.cpp:83`, used at `windowbridge.cpp:477`) walks the tree at
  a time, composing obbs, without drawing. The view walk below has the
  same shape.
- **Every node's obb is an `Animated<Obb>`**
  (`src/avg/include/avg/rendertree/rendertreenode.h:35`), drawn at
  `getValue(time)` and composed onto the parent's transform
  (`shapenode.h:72-79`, `src/avg/src/rendertree/transitionnode.cpp:177`).
- **The size at rest is known per node.** `Animated::getFinalValue`
  (`src/avg/include/avg/animated.h:180`) returns the last key frame's
  target, and `RenderTreeNode::getFinalObb`
  (`src/avg/src/rendertree/rendertreenode.cpp:36`) exposes it for the
  obb; `hasAnimationEnded` (`animated.h:219`) says when it is reached. A
  transaction during an animation replaces the key frames
  (`Animated::updated`, `animated.h:262`), so the final value is always
  the current target. Three limits: nothing composes final obbs from the
  root down today, so the walk must (ancestor scale animations change the
  view's pixel size too); an infinite animation (`isInfinite`,
  `animated.h:193`) has no rest, its "final value" being only the last
  key frame; and an entering or leaving `TransitionNode` animates through
  its own obb, which composes like any other.
- **Queues.** A render context has a main queue and a transfer queue
  (`src/ase/include/ase/rendercontext.h:72-73`), nothing more. Submission
  is FIFO across `submit` calls on one queue.
- **Fences, the frame request, the image and its brush** are designed in
  `image-brush.md` and not repeated here; `ase::Texture` facts (shared
  handle, identity equality, fixed size and format, last reference may
  drop on any thread) are in its *Where the code is*.

## The model

```
producer --present(image)--> view state --frame, step 3--> view node draws
                             (shared,                      an image brush
                              thread-safe)                 into the Drawing
```

- **View state** is a small shared object per view: the current image
  (an optional `avg::Image`), the pending one with its optional ready
  fence, the last reconfigure, and the window's waker. It is thread-safe;
  presents from any thread write it under a mutex and wake the window.
- **The view node** is a render-tree node that references the view state
  and, when the `Drawing` is built, fills its shape with an image brush
  over the current image. The node never changes when the image changes,
  so a new image needs no transaction.
- **The `Drawing` stays a value.** Building it reads the current image
  once; the result holds the image like any other brush.

Consequence (proposed): **a new image value forces the `Drawing` to be
rebuilt**, even when nothing animates, because the old `Drawing` holds the
old image. New pixels in the same texture, the common layer 2 case, are
not a new image (`image-brush.md`: pixels are mutable, values are not) and
only repaint. A layer 1 producer cycling through 2-3 textures changes the
image every present and costs one tree draw per such frame. Patching the
image into the kept `Drawing` is an optimisation for later (*Open
questions*).

## Layer 1: the image source

The primitive. The producer owns its images and presents one to a view;
the view shows it from the first frame after it is ready.

```cpp
// Sketch only, not real signatures.
view.present(image);                  // any thread
view.present(image, readyFence);      // any thread; shown once signalled
view.clear();                         // back to "skipped"
view.onReconfigure(handler);          // see Reconfigure
```

- **Who uses it:** hardware video decode, an external engine with its own
  swapchain, an ase engine on its own queue (2-3 images plus a fence,
  *Later*), hardware buffers (*Later*), and layer 2 internally.
- **Present is a mailbox.** The latest present wins; a present that
  arrives before the previous one was shown supersedes it. A present wakes
  the window through the frame request of `image-brush.md`, made safe off
  the loop thread there.
- **The ready fence is optional.** Without one, the producer promises the
  image is complete on the main queue's timeline, which holds for anything
  submitted to the main queue before the present (FIFO). With one, the
  frame shows the previous image until the fence reports signalled
  (proposed; a GPU-side wait is the alternative, *Open questions*). Fences
  are `image-brush.md`'s: state only, queryable from any thread.
- **Old images stay valid while held.** A view keeps its current image,
  and every `Drawing` and in-flight command holds its own copy, so an
  image never vanishes under the paint. What the producer needs on top is
  to know when it may write an image again (*Open questions*).
- **Room for colour information.** The present takes an `avg::Image`,
  whose "later a colour space" (`image-brush.md`) is where colour space,
  range and transfer function go for video and HDR; the first version is
  SDR `FORMAT_SRGBA` only.
- **Size is advisory.** A presented image is mapped onto the view's
  rectangle, stretched if its size differs (proposed, *Open questions*).
  The producer learns the wanted size from reconfigure.
- **Time** is the producer's own: a layer 1 producer is not frozen by
  remote pause. Only layer 2 runs on the window's frame clock.

## Layer 2: the frame callback

For an engine that renders inside our frame, on our main queue. Built on
layer 1: the view owns one colour texture, the engine renders into it, and
the view presents it through layer 1.

```cpp
// Sketch only, not real signatures.
bqui::RenderHandle<CameraId> scene(
        [&engine](bqui::RenderFrame<CameraId>& frame)
        {
            frame.submit(engine.shadowPass(frame));      // shared pre-pass
            for (auto& v : frame.views())
            {
                if (v.reasons().reconfigured)
                    engine.rebuildTargets(v.key(), v.texture());
                frame.submit(engine.render(v.key(), v.texture(),
                        v.rect(), frame));
                v.present();
            }
        });

auto ui = bqui::widget::uniformGrid(...,
        bqui::widget::renderView(scene, CameraId::top),
        bqui::widget::renderView(scene, CameraId::front,
            bqui::shape::rectangle(radius)));

scene.requestFrame();                                     // any thread
```

**Ownership split (decided).** The **view owns the colour texture**: it
creates it at the size and format reconfigure asks for, recreates it on
reconfigure, and hands the engine the texture plus a rectangle each frame.
The rectangle is the whole texture in the first version; it exists so a
pooled or oversized texture (the image's source rectangle) and direct
mode can use part of a target later without an interface change. The
**engine owns its framebuffer, depth buffer and any intermediate
targets**, built around that texture; the view never makes a framebuffer.
The engine states what it wants (format and the like) and the view
creates it.

**The handle** owns the callback; its copies share the callback and a
request registry. Every `renderView` mounts its handle; a handle mounted
by several views in a window runs **at most once per frame** of that
window and lists those views. A handle in two windows runs once in each
window's frame with that window's views. A callback cannot be replaced.
The frame object hands the callback the window's `ase::Frame`, the render
context, `submit` to the main queue, `requestFrame()` (so a callback never
captures its handle, a cycle), and `views()`, in tree order.

**When it runs.** In a frame of a window where the handle is mounted, when
it is due: a frame was requested through the handle (a first mount
counts), or one of its views was reconfigured since its last call. A
request is honoured with no view listed, so an engine can flush work.
Nothing runs otherwise: a hover elsewhere repaints without calling the
engine, and so does a size animation, because reconfigure follows the
size at rest. During that animation the view's image is stretched from
its at-rest size to the animated geometry. This replaces the slot model's
"exact extent every frame", which reallocated and stalled on every frame
of an animation.

**The engine's tick.** An engine that runs continuously requests the next
frame from inside the callback. The callback runs after the window's
transaction, so signal values it sets reach the UI a frame later; the
bridge returns a zero delay when a request or `needsUpdate_` is pending
after the callbacks, because `GenericWindow::frame` overwrites a request
made during the frame with the returned delay. *Why not an App tick:*
`App::run`'s per-tick callback carries no window, frame time or views, and
cadence belongs to windows (*Frame cadence is per-window* in
`docs/decisions.md`).

**`reasons()`, re-examined against this model:**

| Flag | Slot model meaning | Now |
|---|---|---|
| requested | a request is due | **kept** |
| created | first found in a drawing | folded into *reconfigured*: the first reconfigure creates the texture |
| resized | extent changed this frame | folded into *reconfigured*: follows the size at rest |
| revealed | found again after its texture was released | folded into *reconfigured*: visibility is part of the event |
| retargeted | entering or leaving the direct path | **dropped**; returns with direct mode |
| repaint | a direct view on any painted frame | **dropped**; returns with direct mode |

So two flags remain (proposed): *requested* and *reconfigured*, the latter
carrying the event (size now, size at rest, scale, visibility) and a fresh
texture when the size or format changed. A view listed only because the
handle runs for another view has no flag.

**Present (decided: per view).** `present()` in layer 2 means "the view's
texture now holds a new image"; it routes through layer 1 with no fence,
because the engine's submits precede the paint on the same queue. A view
not presented keeps its last image. A late present (after the callback
returns) asserts in debug.

**Rendering in place, single buffered.** The engine overwrites the texture
the current `Drawing` samples. That is safe on one queue: the previous
frame's paint was submitted earlier and FIFO orders it first. The image
value does not change, so no tree draw is needed, only a repaint. Double
buffering is deferred (*Later*).

**Software video decode** fits layer 2 too: the callback uploads the
decoded frame into the view's texture with `pushUpload` and presents.

## Reconfigure

The event carries:

- **size now**: the view's logical size in this frame's tree times the
  accumulated uniform transform scale (no skew exists:
  `src/avg/include/avg/transform.h`);
- **size at rest**: the same composed from final obbs, the size the view
  will have when its animations end;
- **scale factor**: the window's scaling factor; pixel size is size times
  scale, each axis rounded, best effort;
- **visibility**: whether the view is drawn at all (below).

It fires when the **size at rest** (in pixels), the scale factor or the
visibility changes, not on every animation frame. A transaction that
retargets an animation changes the size at rest and fires again, which is
correct. The size at rest is computable from the tree (*Where the code
is*): the view walk composes `getFinalObb` from the root as `draw`
composes `getObbAt`. **Fallback** where it cannot be (an infinite
animation on the view or an ancestor, detectable with `isInfinite`): fire
when the size has stopped changing, on the first frame the composed size
equals the previous frame's (proposed).

For layer 2 the event reaches the callback through `reasons()`; the view
has created the texture before the callback runs. For layer 1 it goes to
the producer's handler (thread: *Open questions*), and the producer
presents images of the new size when ready; until then the old image is
stretched.

**Visibility** (proposed, unverified against culling): a view is visible
when its composed rectangle meets the window and is not wholly outside
the `ClipNode`s on its path (`src/avg/src/rendertree/clipnode.cpp:140`
composes the clip obb). The walk does not mirror the `Drawing`'s rect
culling (`filterElementsByRect`, `src/avg/src/drawing.cpp`) or the
renderer's cull inside backdrop scopes, and knows nothing of occlusion,
so "visible" may be true for a view that paints no pixel. A hidden view
keeps its image (proposed, *Open questions*), so scrolling back shows it
at once.

## Frame order

Proposed in discussion and treated as the working design. In
`WindowBridge::onFrame`, after resize and the transaction:

1. **Collect views.** Walk the render tree at this frame's time, as
   `RenderTree::snapshot` does, recording each live view with its size
   now, size at rest and visibility; compare with the last walk; create or
   recreate layer 2 textures for reconfigured views. Runs when the tree
   changed or is animating; an idle frame reuses the last list. Hooks in
   between the transaction (`windowbridge.cpp:386-387`) and the draw
   (`:389`).
2. **Run the due frame callbacks**, once per due mounted handle, from a
   copy of the list so a callback cannot invalidate the iteration.
   Presents land in the view state.
3. **Build the `Drawing`**: `RenderTree::draw` (`:389-399`), now also when
   any view's current image changed (layer 2 presents, layer 1 presents
   latched now, fences now signalled). A layer 1 present arriving after
   this step shows next frame.
4. **Paint, present and flush**, unchanged (`:401-404`). The return value
   is zero when a request or `needsUpdate_` is pending, as above.

Running callbacks before the `Drawing` is built is what lets a
reconfigured view show its new image in the same frame, with no blank
frame.

## Handle lifetime and threads

- **Ownership.** The handle owns the callback; the widget tree holds the
  handle through each view; the bridge holds its own reference until the
  handle's last mount in that window is gone. Captures must be
  destructible on any thread holding a copy; deferring destruction to the
  loop thread is rejected, as no loop runs at shutdown.
- **Registration.** Per window the bridge keeps a cell per mounted handle,
  attached in the handle's registry (a mutex and weak references): an
  atomic pending flag and a waker, an object the bridge owns that posts to
  the run loop and is cleared when the bridge dies. A request from any
  thread marks the cells and wakes each window once; a request racing a
  closing window does nothing. A cell holds no callback or GPU resource.
- **View state** is shared between the node, the widget's build state and
  a layer 1 producer, and is thread-safe. The layer 2 texture belongs to
  the view state and dies with its last reference.
- **Not covered:** a minimised window makes no frames, so nothing runs
  there; closing a window or the App does not wait for an engine.

## The renderView widget

`renderView(handle, key, options)` and `renderView(handle, key, shape,
options)` for layer 2; a layer 1 form takes an image source in place of a
handle and key (proposed). Both fill with an image brush mapped onto the
widget's rectangle with fit *fill* (`image-brush.md`'s brush default).

- **Unshaped** is a rectangle; **shaped** fills a `shape::AnyShape` as
  `modifier::surface` does, so the view's outline and shadow follow it;
  avg clips only to rectangles, so this is how a viewport is rounded.
- **State.** The build function makes the view state per build, as
  `scrollView` makes its inputs; a rebuilt widget is a new view.
- **Options:** plain values per widget - opaque (proposed default on; it
  sets the image's opaque declaration, so the fill casts shadows like a
  solid one) and the colour format request for layer 2 (SRGBA only for
  now).
- **Layout and input:** stretches (natural size zero); input through the
  usual modifiers, in local logical coordinates.

## Entering, leaving and hidden views

- **Entering.** A new view has no image and is skipped until its first
  present. Its first walk fires reconfigure; in layer 2 the callback runs
  in the same frame and presents before the `Drawing` is built.
- **Leaving.** The widget's view ends in the transaction that removes it,
  but the `TransitionNode` keeps drawing the node until its exit
  animation ends; the node still references the view state, so it shows
  **its last image, frozen**, and the walk no longer lists it to the
  callback (proposed, as before). Unverified, carried over: that bq
  releases a removed subtree's build state in that update; a test in the
  layer 2 stage checks it.
- **Hidden.** A scrolled-out view keeps its image and texture and is
  reconfigured to invisible; the engine may stop rendering it.

## Later: hardware buffers and foreign images

AHardwareBuffer, IOSurface, DMA-BUF and DXGI shared handles arrive through
**layer 1, as images**: ase imports one into an `ase::Texture`, avg wraps
it in an image, and the producer presents it with its own fence. The
first version already allows this: layer 1 takes any image, from any
thread, with an optional fence, keeps old images valid while held, and
leaves colour information to the image. Needs: ase import and external
synchronisation per backend, and the image's colour space.

## Later: an engine on its own ase queue

An engine that renders at its own rate on its own queue is a layer 1
producer with a pool of 2-3 images and a fence per present. The first
version already has the mailbox, the fence and reconfigure as the
producer's size signal. Needs: extra queues in ase (a context has only
main and transfer today), cross-queue fence waits, and the release
notification (*Open questions*).

## Later: direct mode

A later, **opt-in** stage of layer 2, only for ase engines on our queue.
The texture handed to the engine becomes the **UI's own offscreen colour
target**, and the rectangle becomes the view's area in it. The UI dictates
the colour format; the engine keeps its own depth and intermediate
targets, and only its final pass (resolve or tone map) writes the UI
target. The UI never draws to the default framebuffer in the backdrop
design (*Every window frame renders offscreen* in `docs/decisions.md`), so
the engine never sees it either.

- **What the first version does to allow it:** the engine already gets a
  texture plus a rectangle, never a framebuffer, and builds its own
  framebuffer around them; the fallback texture of a view that may go
  direct is created in the UI target's format, so the engine never sees a
  format change between the two paths.
- **Depends on** the ase prework "framebuffer as description", so an
  engine can describe a framebuffer over a target it does not own.
- **Open issue:** ase cannot clamp drawing to the rectangle through a
  framebuffer the engine built itself: either a restricted texture handle
  that carries the rectangle, or trust.
- **Justify by measurement** on a real scene. It saves one view-sized
  composite pass (estimate, unmeasured: 0.1-0.3 ms at 4K on a discrete
  GPU, 1 ms or more on integrated) and one colour texture; it gains no
  latency, since layer 2 already composites in the same frame.
- **Why not in the first version:** deciding whether a view may draw
  direct needs a conservative analysis of the tree (covering, unclipped,
  opaque, not inside an effect input, no earlier snapshot read after it;
  the slot model's qualification rules are the starting point); the
  fallback path must be kept alive for every frame that fails it; and the
  frame order conflicts, because callbacks run before the `Drawing` is
  built while a direct draw belongs inside the paint, after the content
  under the view.
- **Not the answer for full-screen video on low-power devices:** that is
  OS overlay planes, a separate feature.

## Later: double buffering in layer 2

Deferred. Single buffering is safe on one queue (*Layer 2*). A second
texture, alternated per present, would let the engine render while an
older frame still samples, which matters only once rendering leaves the
main queue. The view owning the texture and presenting through layer 1
already allows it: the view would present the other texture.

## Other later stages

- **Timed requests:** a request with a delay, turned into a due time on
  the loop thread; needs a `requestFrame` variant that lowers the next
  frame time instead of setting it to "now".
- **An update phase** on the handle before the window's signal update, so
  values an engine writes reach the UI in the same frame.
- **Widget lifetime callbacks** (mounted, unmounted, shown, hidden) as a
  modifier, unrelated to rendering.
- **Density settings** (a resolution scale or a pixel-size cap).
- **Asynchronous teardown**, waiting for an engine before a window closes.
- **ase gaps for a real 3D engine** (depth compare, culling, stencil,
  per-draw viewport, multisampling, mipmaps, sampler state, instancing):
  ase PRs of their own, not blocking the stages below.

## Testing

Dummy backend, CI on every leg, real frame loop paused and unpaused:
callback submits precede the painter's; call counts (per request, on first
mount, once per frame for many views, once per window for two windows,
none while only the UI animates, none per frame of a size animation, one
when it comes to rest); reconfigure fields under stepped size and scale
animations, an ancestor's scale animation, a retargeted animation and an
infinite one (the fallback); a reconfigured view presented in the same
frame; a layer 1 present from another thread shown next frame, superseded
by a later one, held back by an unsignalled fence; an image change
rebuilding the `Drawing` and a same-texture present not; leaving views
frozen and unlisted; the final call once decided. Pixels (sRGB, row
order, glass over 3D) by hand on real GL, or through `window.screenshot`
once remote-screenshot lands.

## Open questions for the owner

Each carries a proposed default, unconfirmed.

**From the slot model, resolved by the new one:**

- *Slot to view* (owner link or map): **gone** with the slot.
- *Skip the prefix or require the view first*: **gone** with direct views;
  returns with direct mode.
- *Readers of an earlier snapshot*: **gone** likewise; returns with direct
  mode as a disqualifier.

**From the slot model, still open, reworded:**

1. **Per-view engine resources** (framebuffer, depth, intermediates).
   Proposed: a resource slot on the view (`std::shared_ptr<void>`, say)
   destroyed with the view state; no release callback.
2. **Hidden views.** Release the layer 2 texture when hidden, or keep it.
   Proposed: keep it; the engine may skip rendering. Memory pressure may
   change this later.
3. **Opaque by default.** Proposed: yes, per widget.
4. **Key from a signal**, requesting a frame when it changes. Proposed:
   later, as an overload.
5. **Leaving views.** Proposed: frozen last image, unlisted, kept alive by
   the node's reference to the view state.
6. **The final call (`unmounted()`).** A last call with no views when a
   handle's last mount in a window leaves. Proposed: keep it, flagged on
   the frame object, so an engine can drop per-window resources.

**New with this model:**

7. **Who decides the layer 2 format.** The engine requests, the view
   creates. Proposed: SRGBA only in the first version, the request field
   present; a direct-capable view is later fixed to the UI's format.
8. **What a view shows between reconfigure and its first new image.**
   Proposed: the old image, stretched; layer 2 keeps the old texture
   until the first present into the new one.
9. **Reconfigure's thread for layer 1.** Proposed: the handler runs on
   the loop thread at frame step 1, and the latest event is also readable
   from any thread; the producer hops threads itself.
10. **A layer 1 image whose size does not match the view.** Proposed:
    stretched over the view (fit *fill*), no error; size is advisory.
11. **Fence handling.** Show the previous image until the fence is
    signalled (proposed: non-blocking, one frame late at worst), or a
    GPU-side wait before the paint (no extra frame, but a stalled
    producer stalls the UI).
12. **Release notification for layer 1**: when may a producer write an
    image again. Proposed: a per-present future that resolves once the
    view shows a later image and the frames that sampled this one have
    completed (the window frame fence).
13. **Rebuilding the `Drawing` on a new image.** Proposed: rebuild by tree
    draw in the first version; patch the kept `Drawing` only if
    measurement shows the draw costs.
14. **Infinite animations.** Proposed: the "stopped changing" fallback,
    which never fires while the view keeps animating; such a view keeps
    its last reconfigured size.

## Non-goals

- HDR and wide gamut; image brushes on pens, text and region fills.
- UI inside a 3D scene; partial redraw; group opacity; shaped clipping of
  arbitrary widgets; occlusion culling.
- OS overlay planes, a separate feature.

## Staged plan (first version)

Each stage is one PR, after the backdrop / effect-brushes stack (#172-#177,
#190) and the `image-brush.md` stages it names.

1. **avg: the view node and layer 1.** The view state (current image,
   pending image and fence, mailbox, waker hook), the view node drawing an
   image brush or nothing, and the view walk with size now, size at rest
   (composed final obbs, with the stopped-changing fallback) and
   visibility. Depends on image-brush stages 1-2 (image, brush, lowering)
   and 3 (fences). Dummy tests.
2. **bqui: layer 1 in the frame.** Frame steps 1, 3 and 4; the `Drawing`
   rebuilt on an image change; presents from any thread waking the
   window. Depends on stage 1 and image-brush stage 4
   (`App::getRenderContext()`, the thread-safe window frame request).
3. **bqui: layer 2.** The handle, registry, cells and wakers; frame step
   2; the view-owned texture created on reconfigure; `reasons()`; present
   through layer 1; the zero-delay return; leaving views. Depends on
   stage 2.
4. **bqui: the `renderView` widget.** Unshaped and shaped forms, options,
   the layer 1 form; a demo page in `testapp1`, the bqui readme, the
   `AGENTS.md` files and a `docs/decisions.md` entry. Depends on stage 3.
