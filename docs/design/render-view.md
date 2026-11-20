# Design: render views

> **Status: proposed, nothing implemented.** Written against the unmerged
> `origin/effect-brushes` (tip `30b6727b`, 2026-11-14), which every path
> below points at, and builds on `image-brush.md`. Items under *Decided*
> and items marked **decided** in the text were settled with the owner and
> are recorded, not reopened. Items marked **proposed** are this note's
> working design, not confirmed by the owner; items marked **unverified**
> are claims the code has not been checked for. Names are working names;
> the owner chooses the final ones. The API contract will be Doxygen in
> the headers.

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
- **No key; the producer owns view objects.** A view is an object the
  application creates and keeps: an image source in layer 1, a view
  object the engine makes from its handle in layer 2. The widget takes
  that object, never a key, so a widget rebuild is not a new view: the
  texture and the engine's state survive it. There is no key-from-signal
  form; switching cameras is the engine's own business. An image source
  or view object has **at most one mount** (*The renderView widget*).
- **Reconfigure** tells a producer what the view needs: size now, size at
  rest, scale factor and visibility. It fires when the size at rest
  changes, not on every animation frame.
- **Carried over:** a move-only owner **handle** whose frame callback is
  also the engine's tick; per-view `reasons()`; a per-view **present**; layer 2
  time is the window's frame clock, so remote pause and step freeze the
  3D; SDR pixels per `image-brush.md`; "view", not "surface", which the
  effect branch already uses.
- **One render context.** bqui's render views assume the App's single
  render context (`App::getRenderContext()` in `image-brush.md`). A queue
  per window and extra queues for engines are future mapping only.
- **Stability** is aimed for only at the widget and modifier level; the
  render tree, avg and ase may change freely.
- **Deferred, not blocked:** foreign and hardware images, an engine on its
  own ase queue (with fences and release notification), direct mode, and
  double buffering in layer 2. Each has a *Later* section saying what the
  first version does to keep it open.
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

- **View state** is a small shared object behind each image source or
  view object: the current image (an optional `avg::Image`), the latest
  configuration with its change counter, whether it is mounted, and the
  window's waker. It is thread-safe; presents from any thread write it
  under a mutex and wake the window.
- **The view node** is a render-tree node that references the view state
  and, when the `Drawing` is built, fills its shape with an image brush
  over the current image, with the widget's fit and alignment. The node
  never changes when the image changes, so a new image needs no
  transaction.
- **The `Drawing` stays a value.** Building it reads the current image
  once; the result holds the image like any other brush.

**Rebuilding the `Drawing` (decided).** A new image value forces a
rebuild from the tree, because the old `Drawing` holds the old image. New
pixels in the same texture are not a new image (`image-brush.md`), so
layer 2 and same-texture uploads only repaint the window. Only a producer
presenting a different image each time rebuilds, at the cost of the tree
draw every running animation pays (inference, not measured).

## Layer 1: the image source

The primitive. The producer owns its images and presents one to an image
source; the view mounting that source shows it from the next frame.

```cpp
// Sketch only, not real signatures.
bqui::ImageSource video;                 // the producer keeps it
video.present(image);                    // any thread
video.clear();                           // back to "skipped"
video.config();                          // any thread: latest configuration
video.onReconfigure(handler);            // optional wake-up, loop thread

auto ui = bqui::widget::renderView(video);
```

- **Who uses it:** hardware video decode, an external engine with its own
  swapchain, an ase engine on its own queue (*Later*), hardware buffers
  (*Later*), and layer 2 internally.
- **First version (decided): show the last image presented.** Present is
  a mailbox: the latest present wins, and one that arrives before the
  previous one was shown supersedes it. A present wakes the window through
  the frame request of `image-brush.md`, made safe off the loop thread
  there. On our single main queue an upload or draw into an image is
  ordered against the UI's draws (FIFO), so the first version needs
  neither a ready fence nor a release notification; both are designed in
  *Later: an engine on its own ase queue*. The first present takes the
  image only and returns nothing, so a fence argument and a returned
  future can be added later without changing it.
- **Old images stay valid while held.** A view keeps its current image,
  and every `Drawing` and in-flight command holds its own copy, so an
  image never vanishes under the paint.
- **Opacity (decided)** follows the image's own opaque declaration; the
  view has no override. Transparent content must be premultiplied: the
  texture inside an image always holds premultiplied colour
  (`image-brush.md`'s pixel contract), so the engine's final pass writes it.
- **Size is advisory (decided).** A presented image of another size is no
  error: it is mapped onto the view by the widget's fit and alignment
  (*The renderView widget*). The producer learns the wanted size from
  reconfigure and may follow it.
- **Colour information** goes in the image's "later a colour space"
  (`image-brush.md`); the first version is SDR `FORMAT_SRGBA` only.
- **Time** is the producer's own: a layer 1 producer is not frozen by
  remote pause. Only layer 2 runs on the window's frame clock.

## Layer 2: the frame callback

For an engine that renders inside our frame, on our main queue: the view
owns one colour texture, the engine renders into it, and the view
presents it through layer 1.

```cpp
// Sketch only, not real signatures.
class Scene    // owns the handle and its views; the callback captures this
{
public:
    Scene() : handle_([this](bqui::RenderFrame& f) { onFrame(f); }),
              top_(handle_.makeView(bqui::ViewOptions{}, MyTargets{})) {}
    bqui::ViewObject const& top() const { return top_; }

private:
    void onFrame(bqui::RenderFrame& frame)
    {
        frame.submit(engine_.shadowPass(frame));         // shared pre-pass
        for (auto& v : frame.views())
        {
            auto& targets = v.state<MyTargets>();
            if (v.reasons().unmounted)
                targets.release();                       // no texture
            else if (v.reasons().created || v.reasons().resized)
                targets.rebuild(v.texture());
            if (v.texture() && !v.hidden())
            {
                frame.submit(engine_.render(targets, v.texture(),
                        v.rect(), v.sizeNow(), frame));
                v.present();
            }
        }
    }

    Engine engine_;
    bqui::RenderHandle handle_;
    bqui::ViewObject top_;
};
Scene scene;
auto ui = bqui::widget::renderView(scene.top(), bqui::shape::rectangle(r));
```

**View objects (decided).** The engine creates each view object from its
handle and keeps it; `frame.views()` lists those same objects. Options
are fixed at creation:

- **opaque**, the default, or transparent, set explicitly. An opaque view
  declares its image opaque, so the fill casts shadows like a solid one;
  transparent content must be premultiplied (as in layer 1).
- **format**, fixed for the life of the view object. The first version
  supports only 8-bit sRGB with alpha, and the engine writes **linear**
  values: the target encodes on write, so the engine's own gamma would be
  applied twice. A direct-capable view is later pinned to the UI's
  format; a float format gives no HDR while the UI target is 8-bit.

**Per-view engine state (decided).** Typed, attached by the engine at
creation (`MyTargets{}` above) and read back with a typed accessor
(`v.state<MyTargets>()`, a wrong type asserting in debug); no
`shared_ptr<void>` and no release callback. Its lifetime follows the view
object, not the mount, and the view does no locking around it. The engine
frees its GPU targets when the view reports *unmounted* and rebuilds them
on *created* or *resized*; whatever remains is destroyed with the view
object, on whichever thread drops it last.

**Ownership split (decided).** The **view owns the colour texture**: it
creates it at the size reconfigure asks for and the format the options
fix, recreates it when the pixel size changes, and hands the engine the
texture plus a rectangle each frame. It **releases the texture at
unmount** (decided), dropping its reference once the unmount is noticed;
a later remount allocates a new texture and arrives as *created*. A
hidden view keeps its texture. The rectangle is the whole texture in the
first version; it lets an oversized texture and direct mode use part of
a target later without an interface change. The **engine owns its
framebuffer, depth buffer and intermediate targets**, built around that
texture.

**The handle** is the owner of the callback and a request registry, and
is move-only; widgets and view objects refer to the shared state behind
it, not to the owner. A handle runs **at most once per frame** of each window
where its views are mounted, listing that window's views. A callback
cannot be replaced. The frame object hands it the window's `ase::Frame`,
the render context, `submit` to the main queue, `requestFrame()` (so a
callback never needs its handle), and `views()`, in tree order.

**Destroying the owner (decided)** clears the callback; if it is running
on the loop thread, destruction waits for it to return. The one rule: the
engine is never destroyed from inside its own callback. Views still
mounted keep showing their last image and are never called again.
Proposed, not decided: such views still mount (showing nothing if never
presented), and nothing can make a new view for the dead owner.

**The reference cycle (decided: a documented rule).** Clearing the
callback releases everything it captured, so a view object captured by
value lives until the owner dies, not for the life of the process. The
hard rule: the callback must not own its owner handle, directly or
through a shared pointer to the engine; move-only makes that hard by
accident. The callback reaches views through `frame.views()` and tells
them apart by their typed state. Capturing `this` of an engine class
owning the handle and its views (the sketch) is safe because members die
in reverse order: `top_` first, then `handle_`, whose destructor clears
the callback while `engine_` is still alive. The sketch relies on the
callback touching only members declared before the handle.

**When it runs.** In a frame of a window where the handle has views, when
it is due: a frame was requested through the handle, or a view's
configuration changed since the last call (created, resized, hidden,
revealed or unmounted). A request is honoured with no view listed, so an
engine can flush work. Nothing runs otherwise: a hover elsewhere
repaints without calling the engine, and so does a size animation,
because reconfigure follows the size at rest. Every listed view carries
its **size now**, so an engine that runs anyway corrects its camera
aspect while the image stretches.

**The engine's tick.** An engine that runs continuously requests the next
frame from inside the callback. The callback runs after the window's
transaction, so signal values it sets reach the UI a frame later; the
bridge returns a zero delay when a request or `needsUpdate_` is pending
after the callbacks, because `GenericWindow::frame` overwrites a request
made during the frame with the returned delay. *Why not an App tick:*
`App::run`'s per-tick callback carries no window, frame time or views, and
cadence belongs to windows (*Frame cadence is per-window* in
`docs/decisions.md`).

**`reasons()` (decided):**

- *created*: first listing of this mount, with its texture; the engine
  builds its targets;
- *resized*: the view has a new texture; the engine rebuilds its targets;
- *revealed*: hidden before, visible now; the engine redraws, and
  rebuilds only if it freed its targets while hidden;
- *unmounted*: listed once; no texture;
- *requested*: a request through the handle is due.

Hidden is a state, `v.hidden()`: becoming hidden runs the callback with
no flag. A scale change alters the pixel size, so it arrives as
*resized*; the scale is readable on the view. There is no *reconfigured*
flag, which mixed a replaced texture with a visibility change;
*retargeted* and *repaint* return with direct mode. A view listed only
because the handle runs for another view has no flag.

**Present (decided: per view).** `present()` in layer 2 means "the view's
texture now holds a new image"; it routes through layer 1, needing no
fence because the engine's submits precede the paint on the same queue. A
view not presented keeps its last image. A late present (after the
callback returns) asserts in debug.

**Rendering in place, single buffered.** The engine overwrites the texture
the current `Drawing` samples, safe on one queue because FIFO orders the
previous frame's paint first; the image value is unchanged, so only a
repaint follows. Software video decode fits too: `pushUpload` into the
view's texture, then present.

## Reconfigure

The configuration carries:

- **size now**: the view's logical size in this frame's tree times the
  accumulated uniform transform scale (no skew exists:
  `src/avg/include/avg/transform.h`);
- **size at rest**: the same composed from final obbs, the size the view
  will have when its animations end;
- **scale factor**: the window's scaling factor; pixel size is size times
  scale, each axis rounded, best effort;
- **visibility**: whether the view is drawn at all (below).

Reconfigure fires when the **size at rest** (in pixels), the scale factor
or the visibility changes, not on every animation frame. A transaction
that retargets an animation changes the size at rest and fires again,
which is correct. The size at rest is computable from the tree (*Where
the code is*): the view walk composes `getFinalObb` from the root as
`draw` composes `getObbAt`.

**Infinite animations (decided).** Where the size at rest cannot be
computed (an infinite animation on the view or an ancestor, detectable
with `isInfinite`), reconfigure fires when the size has stopped changing,
on the first frame the composed size equals the previous frame's. A view
under a never-ending size animation keeps its last configured size,
stretched, and a layer 2 engine corrects its aspect from the size now.
The oversized-texture rectangle (*Other later stages*) is the answer if a
real case appears.

**Delivery (decided).** In layer 2 the configuration reaches the callback
through `reasons()` and the listed view; the view has created the texture
before the callback runs. In layer 1 **the stored state is the truth**:
the image source holds the latest configuration, readable from any
thread, with a counter that increases on every change. An optional
handler runs on the loop thread when it changes and is only a wake-up,
"look at the state again"; intermediate configurations may be skipped and
nothing is queued. Unlike fences (*Later: an engine on its own ase
queue*), running it on the loop thread is fine: that is where application
code already runs; the concern with fences was exposing the GL thread.

**Until the first new image (decided)** the view shows the old image,
stretched. Layer 2 normally has no gap (texture created in frame step 1,
rendered in step 2, used in step 3); an engine that skips presenting
leaves the view on its old texture until the first present into the new
one. An animated resize allocates at the size at rest and stretches
until the animation ends.

**Visibility.** In the first version **hidden means clipped out or the
window not shown** (decided), not occluded by another widget; whether
the walk can tell more is unverified. The window half cannot be met
today (*A hidden window*), so in the first version hidden means clipped
out, plus a window hidden with `setVisible(false)` if bqui ever exposes
it. Proposed test: the composed
rectangle misses the window or lies wholly outside the `ClipNode`s on
its path (`src/avg/src/rendertree/clipnode.cpp:140`). The walk does not
mirror the `Drawing`'s rect culling (`filterElementsByRect`) or the cull
inside backdrop scopes, so a "visible" view may paint no pixel.

**A hidden window (decided).** A minimised or hidden window makes no
frames, so no walk marks its views hidden; instead its hide and show
update its mounted views' configuration on the loop thread, without a
frame. Layer 1: the counter increases and the handler runs, so a
producer can pause and resume. Layer 2: the hidden state changes, no
callback is forced, and the first frame after the return lists the views
as *revealed*, harmless though the engine was not called while hidden.
What counts as hidden (minimised, fully covered) is what the platform
reports.

**What ase reports today, read in code: nothing.** `isVisible()` returns
a flag only `setVisible` writes, on every backend
(`src/ase/include/ase/window.h:50-51`); no callback or state reports
minimised, unmapped or occluded. GLX receives map and property events
but drops them and does not select visibility events
(`src/ase/src/glx/glxwindow.cpp:96-100`, `:341-344`, `:368-370`); WGL's
`WM_SIZE` ignores `SIZE_MINIMIZED` (`src/ase/src/windows/wglwindow.cpp:629-631`).
The frame loop never checks visibility (`src/ase/src/platformbase.cpp:163-218`),
so a minimised window still makes frames whenever one is requested.
bqui's `Window` has no show or hide (`src/bqui/include/bqui/window.h:29-73`),
and the bridge shows the window once and never hides it
(`src/bqui/src/windowbridge.cpp:58`).

So two assumptions above do not hold today: a minimised window does make
frames, and nothing flags the views of a hidden window. The decided
mechanism has no hide or show to react to. Window-level hidden needs ase
work first: GLX and WGL tracking minimise and unmap, and ase exposing
it. That joins the ase prework; until then the views of a minimised
window stay visible to the engine.

Inferred, not run: minimising on Windows sends a 0x0 `WM_SIZE`, bqui
lays out at zero size, and `Painter::setSize` divides by the size
(`src/avg/src/painter.cpp:255-256`), filling the projection with
infinities. An existing bug, independent of this design.

## Frame order

Proposed in discussion and treated as the working design. In
`WindowBridge::onFrame`, after resize and the transaction:

1. **Collect views.** Walk the render tree at this frame's time, as
   `RenderTree::snapshot` does, recording each mounted view object (a
   leaving node is not a mount) with its size now, size at rest and
   visibility. Compare the set with the previous frame's **by view
   object**, not by widget or tree node (decided): in both, continuing,
   flagged only for what changed (*resized* if the new mount's size
   differs); only in the new set, *created*; only in the old,
   *unmounted*, and the view object drops its texture. A widget rebuild
   that removes and re-adds the same object in one update thus reports
   nothing. Second mounts are found here (*The renderView widget*). Update
   each changed configuration and its counter; create layer 2 textures
   for created and resized views. Runs when the tree changed or is
   animating; an idle frame reuses the last list. Hooks in between the
   transaction (`windowbridge.cpp:386-387`) and the draw (`:389`).
2. **Run the due frame callbacks**, once per due handle, from a copy of
   the list so a callback cannot invalidate the iteration. Presents land
   in the view state.
3. **Build the `Drawing`**: `RenderTree::draw` (`:389-399`), now also when
   any view's current image value changed. A layer 1 present arriving
   after this step shows next frame.
4. **Paint, present and flush**, unchanged (`:401-404`). The return value
   is zero when a request or `needsUpdate_` is pending, as above.

Running callbacks before the `Drawing` is built lets a created or resized
view show its new image in the same frame.

## Handle lifetime and threads

- **Ownership.** The application keeps its image sources and view
  objects; a widget holds the object it mounts and its node references the
  object's view state. A view object references its handle's shared
  state, and the bridge holds that state until its last mounted view in
  that window is gone. Captures and per-view engine state must be
  destructible on any thread holding the last copy; deferring destruction
  to the loop thread is rejected, as no loop runs at shutdown.
- **Registration.** Per window the bridge keeps a cell per mounted handle,
  attached in the handle's registry (a mutex and weak references): an
  atomic pending flag and a waker, an object the bridge owns that posts to
  the run loop and is cleared when the bridge dies. A request from any
  thread marks the cells and wakes each window once; a request racing a
  closing window does nothing. A cell holds no callback or GPU resource.
- **Mounted** is readable on the view object from any thread. It turns
  false in the first frame whose walk no longer finds the object.
- **Not covered:** closing a window or the App does not wait for an
  engine, and the last callback with its *unmounted* listing may never
  run, which is harmless because view objects release their state when
  destroyed.

## The renderView widget

`renderView(object, options)` and `renderView(object, shape, options)`,
where the object is an image source (layer 1) or a view object (layer 2);
one form serves both layers (decided).

- **Unshaped** is a rectangle; **shaped** fills a `shape::AnyShape` as
  `modifier::surface` does, so the view's outline and shadow follow it;
  avg clips only to rectangles, so this is how a viewport is rounded.
- **Options (decided):** the image brush's **fit** (fill, contain or
  cover, `image-brush.md`) and **alignment**. The view's default fit is
  **fill**, unlike `widget::image`, which defaults to contain; the brush
  itself defaults to fill. Under contain the uncovered area is left
  transparent: put a background behind the view, which has no background
  option. Opacity and format are not widget options: layer 1 follows the
  image, layer 2 the view object.
- **State.** The widget keeps no view state of its own; a rebuild mounts
  the same object again, so the texture and engine state survive it.
- **One mount (decided)**, for image sources and view objects alike: two
  mounts would mean two sizes and visibilities for one configuration. To
  show a picture twice, present the same image to two sources, or draw
  the image value with an image brush or `widget::image`. The build
  cannot reject a second mount (one object passed to two `renderView`
  calls is fine while only one is in the tree), so frame step 1 detects
  it: the first mount in tree order wins; any further one, in any window,
  shows nothing and asserts in debug builds. No exception, which would be
  thrown inside the frame.
- **Layout and input:** stretches (natural size zero); input through the
  usual modifiers, in local logical coordinates.

## Entering, leaving and hidden views

- **Entering.** A mount fires reconfigure; in layer 2 the view is listed
  as *created* and presented before the `Drawing` is built. A view with
  no image is skipped until its first present.
- **Leaving (decided: frozen in the first version).** Once frame step 1
  no longer finds the view object, the next callback lists it once as
  *unmounted* and the object drops its texture. The `TransitionNode`
  keeps drawing the leaving node until its exit animation ends, with the
  image it held at removal, whose value keeps the texture alive. A
  leaving node is not a mount, so the object may be mounted again
  meanwhile. Remounted in the same update, it keeps its texture and the
  leaving node shows live content if that texture is unchanged
  (accepted); remounted later, it has a new texture and the leaving node
  stays frozen. Unverified, carried over: that bq releases a
  removed subtree's build state in that update; a test in the layer 2
  stage checks it.
- **Hidden (decided).** A view keeps its texture and stays in
  `frame.views()`, flagged hidden; the engine skips drawing it and decides
  about its own targets. A layer 1 producer sees it and may pause. Until
  ase reports hidden windows, only clipping flags a view (*A hidden
  window*).

## Later: hardware buffers and foreign images

AHardwareBuffer, IOSurface, DMA-BUF and DXGI shared handles arrive through
**layer 1, as images**: ase imports one into an `ase::Texture`, avg wraps
it in an image, and the producer presents it with its own fence. The
first version already allows this: layer 1 takes any image, from any
thread, keeps old images valid while held, leaves colour information to
the image, and its present can gain a fence. Needs: ase import and
external synchronisation per backend, the image's colour space, and the
fence handling below.

## Later: an engine on its own ase queue

An engine at its own rate on its own queue is a layer 1 producer with
2-3 images and a fence per present. Off the main queue nothing orders its
writes against the UI's draws, so this stage adds (decided design):

- **Fence handling: skip until signalled.** A present may carry a ready
  fence (`image-brush.md`'s: state only, queryable from any thread). The
  view keeps showing the previous image and switches once the fence's
  query says signalled, checked when the `Drawing` is built. Nothing
  polls: the backend wakes the window's loop when a fence completes. A
  present without a fence is shown at once. The view holds at most three
  images: **shown**; **pending**, waiting for its fence and never
  replaced; **next**, the newest present after that, replaced by any
  newer one. When pending signals it becomes shown and next becomes
  pending. The UI never waits on a producer's fence.
- **Release notification.** An image is safe to reuse once the view shows
  a later image, every window frame that drew it has finished (the window
  frame fence), and no leaving node still holds it. Each present returns a
  future that resolves then, on the thread pool; an image dropped from
  *next* unshown resolves at once.
- **Needs:** extra queues in ase (a context has only main and transfer
  today) and cross-queue fence waits.
- **Open issue:** `image-brush.md` has a completing fence wake "the owning
  window's loop"; a fence on an engine's own queue has no owning window,
  so the wake must reach the windows showing the view.

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
  framebuffer around them; a direct-capable view object is pinned to the
  UI target's format, so the engine never sees a format change between
  the two paths.
- **Depends on** the ase prework "framebuffer as description", so an
  engine can describe a framebuffer over a target it does not own.
- **Open issue:** ase cannot clamp drawing to the rectangle through a
  framebuffer the engine built itself: either a restricted texture handle
  that carries the rectangle, or trust.
- **Justify by measurement** on a real scene. It saves one view-sized
  composite pass (estimate, unmeasured: 0.1-0.3 ms at 4K on a discrete
  GPU, 1 ms or more on integrated) and one colour texture; it gains no
  latency.
- **Why not in the first version:** it needs a conservative analysis of
  the tree (covering, unclipped, opaque, not inside an effect input, no
  earlier snapshot read after it), a live fallback for every frame that
  fails it, and a direct draw inside the paint, after the content under
  the view, while callbacks run before the `Drawing` is built.
- Full-screen video on low-power devices is OS overlay planes instead.

## Other later stages

- **Oversized textures for animated resizes:** allocate at the larger of
  the current and the at-rest size and hand out a rectangle of the
  current size each frame; growing allocates at the at-rest size at once,
  shrinking keeps the larger texture until the animation ends. Also the
  answer for never-ending size animations.
- **Double buffering in layer 2:** a second texture alternated per
  present, useful only once rendering leaves the main queue; the view
  would simply present the other texture.
- **Rendering while leaving:** a per-view option to keep a leaving view
  live during its exit animation instead of frozen.
- **Patching the kept `Drawing`** with a new image instead of a tree
  draw, only if measured.
- **Timed requests**, needing a `requestFrame` variant that lowers the
  next frame time instead of setting it to "now".
- **An update phase** on the handle before the window's signal update, so
  values an engine writes reach the UI in the same frame.
- **Widget lifetime callbacks** as a modifier; **density settings**;
  **asynchronous teardown**, waiting for an engine before a window closes.
- **ase gaps for a real 3D engine** (depth compare, culling, stencil,
  per-draw viewport, multisampling, mipmaps, sampler state, instancing):
  ase PRs of their own, not blocking the stages below.

## Testing

Dummy backend, CI on every leg, real frame loop paused and unpaused:
submit order; call counts (per request, on mount, once per frame for many
views and per window, none while only the UI animates or during a size
animation, one at rest, one for an unmount with no views left);
`reasons()` through mount, resize, scale, hide, reveal and unmount; a
same-update remount reporting nothing and keeping texture and state, a
later one getting a new texture; second mounts (both layers, across
windows) showing nothing and asserting; window hide and show updating the
configuration without a frame, once ase reports them; the
configuration under size, scale, ancestor, retargeted and infinite
animations; layer 1 presents from another thread, superseded; the
`Drawing` rebuilt only on a new image value; fit; leaving views frozen.
Pixels (sRGB, linear writes, row order, glass over 3D) by hand on real
GL, or through `window.screenshot` once it lands.

## Lookups before implementation

None remain. The one listed, what ase reports for a hidden, minimised or
occluded window, is answered in *A hidden window*: nothing beyond the
app's own `setVisible` flag. Work it found: ase tracking minimise and
unmap in GLX and WGL and exposing it, as ase prework, before window-level
hidden; and the zero-size resize on a Windows minimise.

## Open questions for the owner

None are open; see *Lookups before implementation* and the open issue in
*Later: an engine on its own ase queue*.

## Non-goals

- HDR and wide gamut; image brushes on pens, text and region fills.
- UI inside a 3D scene; partial redraw; group opacity; shaped clipping of
  arbitrary widgets; occlusion culling.
- OS overlay planes, a separate feature.

## Staged plan (first version)

Each stage is one PR, after the backdrop / effect-brushes stack (#172-#177,
#190) and the `image-brush.md` stages it names.

1. **avg: the view node and its state.** The view state (current image,
   mailbox, configuration with counter, mounted flag, waker hook), the
   view node drawing an image brush with fit and alignment or nothing, and
   the view walk with size now, size at rest (composed final obbs, with
   the stopped-changing fallback) and visibility. Depends on image-brush
   stages 1-2 (image, brush, lowering). Dummy tests.
2. **bqui: layer 1 in the frame.** The image source object, show the last
   image presented (no fence, no release notification), the configuration
   readable from any thread with its wake-up handler, the one-mount rule,
   window hide and show updating the configuration (once ase reports
   them; until then clipping only);
   frame steps 1, 3 and 4; the `Drawing` rebuilt on an image change;
   presents from any thread waking the window. Depends on stage 1 and
   image-brush stage 4 (`App::getRenderContext()`, the thread-safe window
   frame request).
3. **bqui: layer 2.** The handle, registry, cells and wakers; view objects
   from `makeView` with options (opaque, format) and typed state; frame
   step 2; the view-owned texture created on mount and resize and
   released at unmount; `reasons()`
   including the *unmounted* listing; present through layer 1; the
   zero-delay return; leaving views. Depends on stage 2.
4. **bqui: the `renderView` widget.** One form over an image source or a
   view object, unshaped and shaped, with fit (default fill) and
   alignment; a demo page in `testapp1`, the bqui readme, the `AGENTS.md`
   files and a `docs/decisions.md` entry. Depends on stage 3.
