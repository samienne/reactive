# Design: images and the image brush

> **Status: proposed, nothing implemented.** Written against the unmerged
> `origin/effect-brushes` (tip `30b6727b`, 2026-11-14), which this work
> builds on; every path below points there. Names are working names; the
> owner chooses the final ones. The API contract will be Doxygen in the
> headers. Items under *Decided*, and those marked decided, are the
> owner's; what remains to check is under *Lookups before implementation*.

## Purpose

Applications want pictures in the UI: an icon, an avatar cropped to a
circle, a photo behind glass, a texture they render themselves. This note
designs the plain feature, with no notion of frames, engines or slots: an
**image** value in avg, a third `Brush` kind that paints a shape with it,
helpers that make one from pixels, and a bqui `image` widget. It lands
before the render view (`render-view.md`), which builds on it.

## Decided

- **The texture is storage; the image is meaning.** `ase::Texture` is
  already a handle: shared, compared by identity, with a size and format.
  `avg::Image` is a plain value with no shared state of its own: the
  texture, a source rectangle, a scale, a row order and an opaque
  declaration; later a colour space. Equality is memberwise.
- **Pixels are mutable through ase; nothing else is.** A texture's size
  and format are fixed for its life, as on every API but legacy GL
  respecification (Vulkan, D3D, Metal, WebGPU and GL immutable storage all
  fix them); its pixels are not. Changing them changes no image, brush,
  drawing or render tree, and the painter takes no part in it: the
  `Drawing` stays dumb data. The application then asks for a frame.
- **SDR only.** Pixels are 8-bit sRGB RGBA; HDR is a non-goal.
- **Lands first, on the effect-brushes stack.** It needs that stack's
  `std::variant` `Brush`, its painter and pipeline API, and its nested
  `Drawing`, so it is built once that stack is merged.
- **A repaint is asked of the window.** Frame cadence is per window
  (*Frame cadence is per-window* in `docs/decisions.md`), so after new
  pixels the application asks each window showing the image for a frame;
  there is no App-wide request.
- **A fence is state; watching is the context's.** `pushFence` returns a
  fence that can be queried and waited on from any thread; the render
  context turns a fence into a btl future, which resolves on the thread
  pool, never the GL thread (*Fences*).
- **The render context moves to App**, so it exists from App
  construction and a theme built then can hold an image. bqui assumes
  this single context (`render-architecture.md`, *One render context in
  bqui*); ase still supports several, and in avg an image belongs to the
  context its texture came from.
- **The backend is an App constructor argument**; `App::platform()` as a
  later setter goes. Creating the context on first use was rejected: it
  makes a runtime error depend on call order.
- **The App is constructed and run on one thread**, checked by a debug
  assert in run, because the WGL bootstrap window belongs to the thread
  that made it. A precaution: nothing is verified to break otherwise.
- **Decoding stays out of avg.** A file loader is a later stage, in its
  own optional library, on top of the asynchronous helper.

## Where the code is

Verified at `30b6727b`.

- **No sampled image type exists.** The nearest name is
  `avg::TargetImage` (`src/avg/include/avg/targetimage.h:20`), an
  offscreen render target used by the effect emitter and the painter
  (`effectemitter.h:65`, `:155`, `painter.h:92-156`), not a value a brush
  can sample; the working name `avg::Image` sits beside it. There is no
  decoder; the `libpng` wrap in `subprojects/` serves only FreeType,
  which the root `meson.build` builds with `png=disabled`.
- **Textures.** `ase::Texture` (`src/ase/include/ase/texture.h`) is a
  shared handle compared by identity, made by `RenderContext::makeTexture`
  (`rendercontext.h`) from a size and a format. On GL that calls no GL
  (`src/ase/src/gl/globjectmanager.cpp`); `GlTexture::allocate`
  (`gltexture.cpp`) makes the object on first use, on the render thread,
  with undefined contents, linear filtering, clamp-to-edge and no mipmaps.
  The forward-declared `Async` (`ase/async.h`) is an empty, unused struct.
- **Uploads** are commands: `CommandBuffer::pushUpload` replaces a whole
  texture, `pushSubUpload` a region (`commandbuffer.h`), executed when the
  buffer is submitted to a `RenderQueue`. The glyph atlas pushes its
  sub-uploads into the frame's command buffer before its draws, each
  glyph padded by one texel (`GlyphAtlas::endFrame`,
  `src/avg/src/glyphatlas.cpp`), and treats its first row as the top,
  avg's y axis pointing up (`clipQuad` in `rendering.cpp`).
- **Queues and threads.** A context has a main render queue and a
  transfer queue. On GLX the transfer queue is a second, shared context
  on its own dispatcher thread; WGL disables that
  (`isBackgroundQueueEnabled`, `src/ase/src/windows/wglplatform.cpp:208`),
  so there the transfer queue is the main queue. ase states no
  thread rule for `makeTexture` or `submit`. GL's `submit` only queues a
  closure under the dispatcher's mutex (`GlRenderState::submit` in
  `glrenderstate.cpp`, `src/ase/src/dispatcher.cpp`); the dummy queue's
  submission recording is unguarded (`src/ase/src/dummy/dummyrenderqueue.cpp:35`).
- **Upload completion.** `CommandBuffer::pushFence` returns a
  `btl::future::Future<>` that resolves once the commands before it have
  completed; an overload takes a callback (`src/ase/src/commandbuffer.cpp:50`).
  GL polls its sync objects in `GlRenderState::checkFences` at the end of
  every submit and, while any are pending, from a 16 ms dispatcher idle
  function (`src/ase/src/gl/glrenderstate.cpp:580-628`,
  `src/ase/src/dispatcher.cpp:83-90`), and resolves them on the GL
  thread; the dummy queue resolves inline in `submit`, on the caller's
  thread. `~GlRenderState` (`glrenderstate.cpp:183-194`) drops pending
  promises without resolving or failing them and leaks their sync
  objects, so a waiter hangs. The one user is the window frame fence
  (`WindowBase::submitFrameFence`, `src/ase/src/windowbase.cpp:56-79`,
  pushed each frame from `platformbase.cpp:180`): its callback frees a
  backpressure slot and calls the thread-safe `requestFrame`; on dummy
  the inline resolve frees the slot before the next tick. Nothing in
  avg, bqui or the tests pushes a fence.
- **Futures.** `btl::future::Future` (`src/btl/include/btl/future/`) has
  `then`, `onFailure` and failure by `std::exception_ptr`; continuations
  run on the setting thread (`FutureControl::set`); `btl::async` runs work
  on the shared thread pool, and `ThreadPool::getInstance().async`
  (`src/btl/include/btl/threadpool.h`) posts a plain callable; the pool
  is a function-local static joined at exit, with no earlier shutdown.
  ase already depends on btl. bq has no adapter
  from a future to a signal; an input's `set` is mutex-guarded and fires
  observers on the calling thread (`src/bq/include/bq/signal/input.h`).
- **`Brush`** (`src/avg/include/avg/brush.h`, `src/avg/src/brush.cpp`)
  is a variant of `Color` and `Effect`. `operator*(Transform, Brush)` only
  passes the transform's scale on (`transformElement` and `scaleElement`
  in `src/avg/src/drawing.cpp` bake a transform into the shape and
  multiply the brush); `hash_append` treats every non-colour as an effect.
- **Painter** (`src/avg/src/painter.cpp`): every pipeline draws into
  `FORMAT_SRGBA`; the solid pipeline has no blend, the transparent and
  text pipelines blend `One, OneMinusSrcAlpha`. `renderElements`
  (`rendering.cpp`) puts unblended elements first and batches while
  pipeline and textures stay the same. Alpha is premultiplied and colours
  are linear: the target is sRGB with `GL_FRAMEBUFFER_SRGB` on (*Every
  window frame renders offscreen* in `docs/decisions.md`). Outlines ask
  `isOpaque` (`src/bqui/src/shape/shape.cpp`) and `isOpaqueColour`
  (`src/avg/src/rendertree/surfacenode.cpp`).
- **Repaints.** bqui has no public frame request. `WindowBridge`
  (`src/bqui/src/windowbridge.cpp`) requests a frame of its ase window
  from its signal wakeups, and `onFrame` paints the current drawing even
  when the tree did not change. That request races today:
  `GenericWindow::requestFrame` sets `nextFrame_` unguarded
  (`src/ase/src/genericwindow.cpp`), the loop reads it in its tick, and
  `SignalContext::observe` runs a wakeup on the thread that made the
  change, so a worker setting an input a window reads races the loop.
- **Windows and context.** `bqui::Window` shares a `WindowData`
  (`src/bqui/src/windowdata.h`) with a weak reference to the App, but the
  bridge lives in the App's private list, so it cannot reach a frame
  request. The platform, run loop and one render context are made in
  `App::runUntil` (`src/bqui/src/app.cpp:331-363`), shared by the windows
  and exposed to no one; `App::platform()` (`app.cpp:282`) can pick the
  backend after construction.

## The image

`avg::Image` is a value of:

- its **texture**, an `ase::Texture` of format `FORMAT_SRGBA`, which
  carries the identity, the size and the format; the image stores none of
  them again;
- its **source rectangle**, whole texels inside the texture, the whole
  texture by default. The image's **size** is the rectangle's size;
- its **scale**: pixels per logical unit, 1 by default, 2 for an asset
  drawn at twice the density. The **natural size** (decided) is size /
  scale in logical units; layout never depends on the display. An image
  made from raw pixels with no stated scale takes one unit per pixel and
  draws upscaled, soft, on a 2x display; the fix is a larger image with
  its scale stated. Picking a 1x or 2x variant for the display is a
  loader feature, not this note's;
- its **row order**: top row first (uploaded data, as the glyph atlas) or
  bottom row first (a texture rendered with clip-space +y up);
- an **opaque** declaration, made by the application: it promises every
  pixel it will ever write inside the rectangle has alpha 1. False by
  default.

Equality, hashing and ordering are memberwise, the texture by identity:
two images of the same texture and rectangle are equal, two made from
identical bytes are not, and two rectangles of one atlas are different
images. Making an image from an existing texture needs no render context;
a texture of another format asserts in debug and draws nothing in
release. In ase, windows carry their render context and may use
different ones (`render-architecture.md`), so in avg an image belongs to
the context its texture came from and draws only on that context; bqui
uses the App's single context, so there every window can draw every
image. An image can be a signal value, a `std::optional`, a member of a
theme; there is no empty image, "no image" is `std::optional<Image>`.

**The source rectangle** serves icon atlases and sprite sheets, and lets
a later render view keep an oversized or pooled texture and change only
the rectangle on a resize. It is in the texture's own texel coordinates,
row 0 the first row in memory, so it means the same whatever the row
order; the row order then says whether the rectangle's first row is the
image's top or its bottom, and flipping happens inside the rectangle.

## Changing pixels

Pixels change through ase: an upload, a sub-upload or a draw into the
texture, submitted to a queue of the context. That changes no value, so
nothing in avg notices: the next frame that draws the image samples
whatever the texture then holds. A queue executes in order, so work
submitted to the main queue before a frame's draws is seen by them, so
updating an existing image is already same-frame. Rendering on another thread, another queue or inside the frame is the
render view's business. The writes must follow the pixel contract (avg
offers the conversion from straight alpha), and a texture nothing has
written is undefined, so an application fills its own before showing it.

## Making an image from pixels

The texture needs the render context; the image does not. bqui gains
`App::getRenderContext()`, the easy way to reach the App's default
context, valid from App construction (the render view needs it too).
An avg helper
takes a context, a size and tightly packed 8-bit sRGB RGBA rows, top row
first, with **straight** alpha (what decoders produce). It converts the
pixels on the calling thread, makes the texture, submits a whole upload
with a fence after it, watches the fence through the context and returns
a `btl::future::Future` of the image, chained by `then` on the watch: it
resolves once the upload has finished, so nothing shows a blank texture.
A bad size or a short buffer fails it. It wraps upload, fence and watch,
so application code rarely touches a fence. Decided:

- **Straight alpha only.** Accepting premultiplied bytes is one optional
  parameter, addable later without changing existing calls; not in the
  first version.
- **No synchronous form.** Only first creation is asynchronous (updates
  are same-frame, above). A size-bounded synchronous form may come later,
  justified by a real case the update path cannot serve; its cost is
  unbounded otherwise (the pixels are copied at record time and again on
  the GL thread; a 4K RGBA image is about 33 MB).
- **The main queue uploads, for now.** It orders the upload before later
  draws for free, at the cost of render-thread time: a large upload while
  the UI animates may make a frame late. The transfer queue follows with
  the ase prework (the fence redesign for a reliable cross-context
  "upload complete", queue roles, WGL given a real second context like
  GLX), once ase states when transfer-queue work becomes visible to
  main-queue draws and a readback test confirms it. Callers get a future
  either way, so no application code changes. Motivation, a review
  finding not confirmed at runtime: transfer-context state changed after
  its last flush is not guaranteed visible to the main context
  (`src/ase/src/gl/glprogram.cpp:336` vs `:344` on the shader branch).
- **Any thread.** ase guarantees that creating a texture and submitting a
  finished command buffer are safe from any thread. Building one command
  buffer stays single-threaded (each thread builds its own); submissions
  from one thread keep their order, between threads the order is arrival.
  The dummy queue's recording gains a lock. GL is safe today as read from
  the code (*Where the code is*), not by a stated guarantee or a test.
  Hopping the helper to the loop thread was rejected.

### Fences

A sketch of the shape, not real signatures:

```cpp
CommandBuffer cb;
cb.pushUpload(texture, pixels);
ase::Fence fence = cb.pushFence();    // state only, unsignalled
context.getMainQueue().submit(std::move(cb));
context.watch(fence).then([] { /* on the pool */ });
fence.isSignalled();                  // any thread, never blocks
fence.wait();                         // any thread, blocks
```

- **A fence is state only:** a query and a blocking wait, both callable
  from any thread. `pushFence` returns it; the callback overload goes.
- **Watching is the render context's.** Given a fence it returns a btl
  future, to which the caller attaches `then`; no callback is handed to
  ase. The context is the one obvious object (what
  `App::getRenderContext()` returns) and owns the queues; a fence is
  bound to its queue when its buffer is submitted, so the context routes
  the watch and no wrong queue can be named. Watching before submit is
  fine: the context holds the watch until the fence is bound.
- **Signalled** means the buffer was submitted and the backend completed
  it; until then a fence reports unsignalled. A fence whose buffer is
  dropped unsubmitted, or whose queue or render state is torn down, fails
  its watchers and releases its blocked waiters. This fixes the hang in
  `~GlRenderState` (*Where the code is*).
- **On completion a backend does at most three things:** marks the fence
  signalled, wakes the owning window's loop (ase's own code, one
  thread-safe call) and hands any watched promise to the btl pool. An
  unwatched fence costs only its state.
- **The frame fence** stops using a callback: the window counts its
  unsignalled fences on its tick and relies on the wake. The dummy
  backend signals inline in `submit`, so headless frame stepping stays
  deterministic.

**Query, wait and watch are backend operations**, virtual behind ase's
handle and impl split. How a backend notices completion is its own
business and the interface promises none; no backend gets a thread
dedicated to waiting. GL keeps its poll: GL sync objects can be asked
only on a context thread, so the query reads a flag the poll sets and
the blocking wait waits on a condition the dispatcher sets. Later
backends, from general API knowledge and not verified:

- **Vulkan** queries from any thread and blocks on several fences
  natively; it notices by a run loop timer poll or an exported fd, and a
  timeline semaphore answers every fence on a queue with one counter.
- **D3D12** sets an event on completion.
- **Metal and WebGPU** have their own completion callbacks.

Where noticing depends on the run loop, a watched future does not
resolve until the loop runs; that is why the blocking wait is a backend
operation of its own.

**Watched futures resolve on the thread pool, never on the GL thread.**
On the GL thread a slow continuation would stall rendering for every
window, one that submits and waits on another fence would deadlock, its
captures would die there, and queueing more work onto that thread by
accident would be easy. The pool rather than the loop thread: `wait()`
on the main thread keeps working, where a loop resolve would deadlock
it; on GL nothing needs a running loop, which images made before
`App::run` want; and it matches `btl::async`. The way to the UI is bq's
signal from a future. The window frame request is safe from any thread
(next section); setting a bq input is too, as read, with the wake it
causes as the weak link (*Lookups before implementation*).

The costs: resolution is not ordered against frames, so tests wait on
the future instead of assuming an inline resolve, and a `then` that
touches UI state runs on a pool thread. On GL a fence is noticed up to
about 16 ms late while the dispatcher is idle. On WGL the transfer queue
is the main queue, as are image uploads for now, so a large upload
delays rendering. The pool is
joined at static destruction, so no fence may complete after `main`
returns; the render context and its dispatchers end with the App,
before that.

**A placeholder** needs a signal that starts pending and changes when
the future resolves; the widget shows the placeholder while it is
pending. bq gains that small adapter (decided; a working name: signal
from future), generic over any value: a future becomes a signal of a
three-state value, pending, ready or failed. Its continuation holds only the input's weak
handle, so dropping the UI before the upload ends is harmless. bqui
builds the widget form on it: a future of a widget plus a default widget
gives a widget. The wider async design (explicit loading, a default font,
loading before the window opens) and the adapter's value type are in
[async-resources.md](async-resources.md).

## Asking for a repaint

Changing pixels changes no value the UI reads, so no signal wakes a
window; today an application can only force a frame by changing some
signal a window reads. bqui's `Window` gains a **frame request** (a
working name), callable from any thread, that marks that window due and
wakes the loop; the window then repaints its unchanged drawing, which
samples the new pixels. An image shown in several windows needs a
request to each; a window not asked keeps the old pixels until it next
repaints. A request to a window in no app, or one closed, does nothing.

bqui routes the `Window` handle to its bridge through the shared
`WindowData`, set and cleared with the bridge under that object's mutex.
The same stage makes the racy ase request safe off the loop thread
(posted to the run loop, or an atomic due time), which also fixes the
race from signal wakeups and future continuations; on dummy and
offscreen windows it must also wake the platform.

## Loading from disk (later stage)

A loader on top of the asynchronous helper, outside avg's core: it reads
and decodes the file on a worker (`btl::async`), uploads through the
helper and resolves with the image; a missing file, a decode error or a
failed upload fails the future. The toolkit has no decoder. The options:
**stb_image** as a Meson wrap, one file for PNG, JPEG and more; **libpng**
and a JPEG library, two heavier dependencies; or an **application decode
callback**, no dependency but glue in every application.
*Recommendation:* stb_image, in the loader's own optional library that
depends on avg, so avg never does; the loader also takes a decode
callback for applications with their own decoder.

## Pixel contract

- **Format.** `FORMAT_SRGBA` only.
- **Colour (decided invariant).** The texture inside an image always
  holds **premultiplied** colour, stored as the sRGB encoding of linear
  values, exactly what an avg render target holds after drawing; the
  painter blends on that assumption. Whoever builds an image directly
  from a texture (a transparent render view, whose engine's final pass
  writes it, for one) must put premultiplied content in it. Sampling decodes to linear and blends `One, OneMinusSrcAlpha`
  like every other element, without dark fringes under filtering.
- **Conversion** from straight alpha, which the raw-byte helper always
  applies before upload: decode each channel to linear,
  multiply by alpha, encode again; opaque and fully transparent pixels
  come out exact, dark translucent ones lose a little precision.
- **Rendered pixels** already follow it when drawn with avg's blending;
  with `GL_FRAMEBUFFER_SRGB` on, a producer writes linear values.
- **Filtering** is the texture's own: linear, no mipmaps, so strong
  minification aliases until ase has sampler state and mip generation.

## The image brush

`Brush` gains a third kind: an **image**, a **mapping** (an `Obb` in the
element's space onto which the image's source rectangle is mapped,
possibly unset), a **fit** with an **alignment**, and an **opaque** flag
that defaults to the image's declaration.

- **Mapping resolution.** A brush with no mapping gets one when its
  `ShapeElement` is made, from the filled shape's bounds and the fit. One
  helper does it on every path that makes an element from a new brush:
  `Shape::fill`, `stroke` and `fillAndStroke` (`src/avg/src/shape.cpp`),
  the two elements in `SurfaceNode::draw` and the one in
  `builtineffects.cpp`. `withResource` in `drawing.cpp` and the text
  fallback copy a resolved (or, for text, unsupported) brush. Lowering
  asserts in debug that an image brush has a mapping.
- **Transforms compose into the mapping.** `operator*(Transform, Brush)`
  and the float scale of `Drawing::operator*` map the mapping, so the
  image follows the shape through later transforms, rotation included;
  an unset brush stays unset. Transforms baked into a shape before it is
  filled are not followed: a rotated shape maps onto its axis-aligned
  bounds.
- **Equality.** Same image, mapping, fit, alignment and flag. New pixels
  leave the brush equal, so the tree does not change. Two image brushes
  are `isSameKind` when they share the image, fit and alignment, and
  `lerp` interpolates their mappings; any other pair, two rectangles of
  one atlas included, switches at once, as `isInstantLerp` already
  arranges for brushes of different kinds.
- **The other sites** that read a non-effect brush as a colour gain the
  image case: `getColor`, ordering (colours, then images, then effects),
  printing and float scale in `brush.cpp`, `hash_append` in `brush.h`,
  `lerp.cpp`, `Painter::getPipeline` and the vertex makers in
  `rendering.cpp`.
- **Lowering.** The fill is tessellated as for a colour. Each vertex gets
  a point `t` in the unit square from its position in the shape's space,
  before the mesh's transform, through the inverse of the mapping; the
  row order flips `t` vertically; the uv is the rectangle's origin plus
  `t` times its size, over the texture's size. Clipping, rotation and
  stretching stay exact. Where the shape reaches outside the mapping
  (*contain*), the fill shape is first intersected with the mapping's
  rectangle, so the edge texels are not smeared over the rest; this is
  skipped when the mapping covers the shape's bounds, the common case.
  Vertex colour is premultiplied white.
- **Edges of the rectangle.** Linear filtering at the rectangle's edge
  reads half a texel beyond it: clamp-to-edge makes that the edge texel
  for a whole texture, but inside an atlas it is the neighbour, which
  bleeds in. So each vertex also carries the rectangle inset by half a
  texel, in uv, and the fragment shader clamps the uv to it, repeating
  the edge texel as clamp-to-edge would. Atlases that want mipmaps later
  still pad, as the glyph atlas does.
- **Two pipelines** follow `makeTextPipeline`: the same target and depth
  formats, the text vertex layout (colour, position, uv) plus the uv
  bounds, and one sampler. The opaque one has no blend and writes alpha
  1, so it sorts with solid colours; the translucent one blends and keeps
  paint order. Draws batch per texture, as `renderElements` enforces, so
  rectangles of one atlas share a batch. Effect scratch targets have the
  same formats, so effect sub-drawings use the same pipelines.
- **Fills only.** An image brush on a pen, a `TextEntry` or a `RegionFill`
  draws nothing and asserts in debug.
- **Free from existing machinery:** clipping, any shape as the fill
  geometry, backdrop scopes and glass over an image.

## Fit

Fit resolves the mapping from the shape's bounds `B` and the image's
natural logical size `S`, the rectangle's size over the scale:

- **fill**: the mapping is `B`; the image stretches.
- **contain**: the largest rectangle of `S`'s aspect inside `B`, placed by
  the alignment; the rest of the shape stays empty.
- **cover**: the smallest rectangle of `S`'s aspect containing `B`,
  placed by the alignment; the overhang is cut by the shape.

Alignment is a point in the unit square, centre by default. An explicit
mapping bypasses fit. New pixels never move a resolved mapping; a new
rectangle is a new image and resolves anew.

Decided here: the **brush defaults to fill**, the one rule that needs no
aspect and the one the render view wants; the **widget defaults to
contain**, which never distorts or crops a picture the layout resizes.

## Opacity and outlines

`isOpaque` and `isOpaqueColour` decide whether a fill adds an outline,
which drives shadows. An opaque image fill adds one and casts a shadow
like a solid colour; a translucent one does not, so an icon with a
transparent border casts no box. Under *contain* the shape is partly
empty, so no outline unless the mapping covers the shape's bounds. The
flag comes from the declaration, not the pixels, which change without the
tree knowing.

## Texture lifetime

Each copy of the image holds a texture handle, so the texture lives
while any copy does: in a brush, a `Drawing`, a render tree node
(including a leaving one in its exit transition) or a signal. A draw
command in flight holds its own `ase::Texture` copies
(`CommandBuffer::push`), so dropping the image mid-frame is safe.

The last copy can drop on any thread, a future's continuation included:
`GlTexture::~GlTexture` hands `glDeleteTextures` to the transfer queue's
dispatcher (`dispatchBg`, `gltexture.cpp`), so that is safe. The hazard:
`GlTexture` holds a plain `GlRenderContext&` (`gltexture.h`), so an image
must not outlive its render context; one kept in a static or a signal
past the App dangles.

## bqui: the image widget and brushes on shapes

- **Any shape.** A shape widget's `fill` already takes a brush or a
  signal of brushes (`src/bqui/include/bqui/shape/shape.h`), so
  `shape::circle().fill(Brush(avatar, Fit::cover))` is an avatar and needs
  no new bqui code beyond the outline rule above.
- **`widget::image(image, fit)`**: a rectangle filled with an image brush,
  whose natural size is the image's natural logical size, so an icon made
  at scale 2 is laid out at half its pixel size. The image may be a
  signal, of the future adapter's three-state value, with a placeholder
  while it is pending (`async-resources.md`, *The future adapter*);
  a new image re-lays out only when its natural size changes. Fit
  defaults to contain, alignment to centre. A rounded image is a shape
  fill, not an option of the widget.
- **`App::getRenderContext()`**, the App's default context, and the
  **window's frame request**, above.

Only the widget and modifier surface aims for stability; `avg::Image`,
the brush layout and the painter internals may change.

## Relation to the render view

The render view builds on this brush: its views are image fills whose
pixels come from an engine, and it can keep an oversized or pooled
texture and change only the source rectangle on a resize. How it holds
its mutable state is being decided separately; this note does not design
it.

## Testing

No pixel readback exists on this stack, so most tests check what the
painter submits on the dummy backend, and pixels are checked by hand.

- **Values (dummy, CI).** `src/avg/test/brushtest.cpp`: memberwise
  equality, order and hash of images and image brushes, printing,
  `isSameKind`, `lerp`, the conversion. `drawtest.cpp`, `shapetest.cpp`:
  resolution on every element path, each fit and alignment from the
  rectangle and scale, composition under transforms and float scale.
- **Lowering (dummy, CI).** `src/avg/test/renderingtest.cpp` inspects the
  commands, as `clipRectCutsQuadsAndTheirTexels` does for glyph quads: uv
  under clipping, rotation, stretching and both row orders, for a whole
  texture and a source rectangle; the half-texel bounds on every vertex;
  the contain intersection; texture and pipeline; two rectangles of one
  atlas in one batch; opaque images first; a wrong format drawing
  nothing. `effectrenderingtest.cpp`: an image in an effect sub-drawing
  and under glass.
- **Helpers (dummy, CI).** One whole upload of converted pixels then a
  fence on the main queue; resolves (waited on, on the pool) with an
  equal image; fails on a short buffer; works from a worker thread, with
  dummy submissions from two threads recorded intact.
- **Fences (dummy, CI).** Unsignalled before submit, signalled inline by
  it; a watch made before submit resolves on the pool; a dropped buffer
  and a torn-down queue fail watchers and release a blocked wait; headless
  frame stepping never finds a window saturated.
  bq's adapter: pending until the future resolves, then the image or the
  failure; a
  dropped signal ignores a late result, driven by `makeManualFuture`.
- **bqui (dummy, CI).** `src/bqui/test/shapetest.cpp` and
  `shadowboundarytest.cpp`: outlines under opaque, translucent and contain
  fills; `widgettest.cpp`: the natural size; `apptest.cpp`: a frame
  request from another thread repaints that window only, tree unchanged.
- **Pixels (real GL, manual).** sRGB round trip, premultiplied edges,
  row order, alpha 1 from the opaque pipeline, an asynchronous image
  never drawn blank, and no bleeding: atlas tiles in distinct colours,
  magnified and minified, show no neighbour's colour at their edges.
  `bquiheadlesstest` renders offscreen by hand; `origin/remote-screenshot`
  reads back once it lands, exactly at texel centres.

## Staged plan

Each stage is one PR, after the effect-brushes stack is merged; readback
tests also wait for remote-screenshot.

1. **avg: the image and the brush.** `Image` as a value with its source
   rectangle, and the conversion; the brush kind with mapping, fit and
   alignment, the resolution helper on every element path, transform and
   float-scale composition, and every brush site except lowering; the
   opacity tests in `surfacenode.cpp` and bqui's `shape.cpp`.
2. **avg: drawing it.** The two pipelines with the uv bounds, lowering
   with uv inside the rectangle and the clamp, the contain intersection,
   the format check. Dummy tests; one manual readback, bleeding included.
3. **ase, avg and bq: making images.** ase's fence object with query,
   wait and the context's watch, failure on drop and teardown, the frame
   fence counting; the any-thread guarantee and the dummy queue's lock;
   the asynchronous image helper on the main queue; bq's signal from a
   future.
4. **bqui: the widget.** `widget::image` with fit, alignment and the
   adapter's three-state value, and the widget form of the future
   adapter; the render context on App with `App::getRenderContext()`,
   the backend as a constructor argument and the same-thread assert in
   run; the frame request on `Window`, with the ase request made safe off
   the loop thread.
   The transfer queue for uploads comes later, with the ase prework.
5. **Later: the loader**, in its own optional library with its decoder.

## Lookups before implementation

Both are done, read in code; detail and references are in
[async-resources.md](async-resources.md) (*Where the code is*).

- **Setting a bq input from another thread is safe** for the value and
  the context: `set` writes under the input's mutex, and the loop thread
  copies the value into its own context data under the same mutex.
  Observers fire on the setter's thread, outside the lock. No test does
  it yet.
- **Whether it causes a frame depends on who reads the input.** Read by
  the app's running signal, the wake is thread safe and causes a frame
  on every platform. Read by a widget, it goes through the racy
  `GenericWindow::requestFrame` (fixed on the local branch
  `bugfix/requestframe-race`), and on dummy and offscreen windows
  (the dummy platform, headless windows, macOS) it does not wake the
  platform, so an idle loop may stay blocked. The frame request stage
  (*Asking for a repaint*) covers both; it also needs a test setting an
  input from another thread.

## Open questions for the owner

None are open; see *Lookups before implementation*, and the claims
marked unverified in place.

## Non-goals

- Decoding image files inside avg; a loader on top is the later stage.
- Partial-upload helpers (ase's `pushSubUpload` serves), HDR, wide gamut,
  `FORMAT_R8` masks, mipmaps, sampler state, tiling and nine-patch images.
- Image brushes on pens, text and region fills; tinting and group opacity.
- Rendering into an image off the loop thread or inside the frame, and
  any per-frame engine callback: the render view.
