# Design: images and the image brush

> **Status: proposed, nothing implemented.** Written against the unmerged
> `origin/effect-brushes` (tip `30b6727b`, 2026-11-14), which this work
> builds on; every path below points there. Names are working names; the
> owner chooses the final ones. The API contract will be Doxygen in the
> headers. What this note could not settle is under *Open questions for
> the owner*.

## Purpose

Applications want pictures in the UI: an icon, an avatar cropped to a
circle, a photo behind glass, a texture they render themselves. This note
designs the plain feature, with no notion of frames, engines or slots: an
**image** handle in avg, a third `Brush` kind that paints a shape with it,
and a bqui `image` widget. It lands before the render view
(`render-surface-widget.md`), which builds on it.

## Decided

- **An image is a handle to pixels.** Its identity, size and format are
  fixed when it is made; its pixels are not. They may be uploaded or
  rendered after the image was made and already used in a brush. Changing
  them changes no brush, drawing or render tree; it only needs a repaint.
- **SDR only.** Pixels are 8-bit sRGB RGBA; HDR is a non-goal.
- **Lands first, on the effect-brushes stack.** It needs that stack's
  `std::variant` `Brush`, its painter and pipeline API, and its nested
  `Drawing`, so it is built once that stack is merged.
- **A repaint is asked of the window.** Frame cadence is per window
  (*Frame cadence is per-window* in `docs/decisions.md`), so after new
  pixels the application asks each window showing the image for a frame;
  there is no App-wide request.

## Where the code is

Verified at `30b6727b`.

- **No image type exists.** A search of `src/` for `Image` finds only a
  comment in `src/avg/include/avg/effectemitter.h`; there is no decoder
  either. Textures are `ase::Texture` (`src/ase/include/ase/texture.h`), a
  shared handle compared by identity (`src/ase/src/texture.cpp`), made by
  `RenderContext::makeTexture` and filled by `CommandBuffer::pushUpload`.
- **`Brush`** (`src/avg/include/avg/brush.h`, `src/avg/src/brush.cpp`)
  is a variant of `Color` and `Effect`. `operator*(Transform, Brush)` only
  passes the transform's scale on; `hash_append` treats every non-colour
  as an effect.
- **Elements are made** by `Shape::fill`, `stroke` and `fillAndStroke`
  (`src/avg/src/shape.cpp`), by `SurfaceNode::draw`
  (`src/avg/src/rendertree/surfacenode.cpp`) and by
  `src/avg/src/builtineffects.cpp`. `drawing.cpp` (`withResource`) and the
  text fallback in `rendering.cpp` rebuild elements from existing brushes.
- **Transforms** reach a `ShapeElement` through `transformElement` and
  `scaleElement` in `src/avg/src/drawing.cpp`: the shape gets the transform
  baked in and the brush is multiplied.
- **Painter** (`src/avg/src/painter.cpp`): every pipeline draws into
  `FORMAT_SRGBA` over the target's depth format. The solid pipeline has no
  blend; the transparent and text pipelines blend `One,
  OneMinusSrcAlpha`. `renderElements` (`src/avg/src/rendering.cpp`) puts
  unblended elements first and batches while pipeline and textures stay
  the same. Glyph uploads are pushed into the frame's command buffer after
  its items are made and before its draws (`GlyphAtlas::endFrame`).
- **Alpha is premultiplied.** `premultiply` in `rendering.cpp` scales a
  colour by its alpha when making vertices, and the text shader scales the
  premultiplied colour by coverage. Colours are linear: the target is sRGB
  with `GL_FRAMEBUFFER_SRGB` on (`src/ase/src/gl/glrenderstate.cpp`;
  *Every window frame renders offscreen* in `docs/decisions.md`).
- **Textures in GL** (`src/ase/src/gl/gltexture.cpp`): `FORMAT_SRGBA` is
  `GL_SRGB8_ALPHA8`, sampled linear with clamp-to-edge and no mipmaps.
  `allocate` passes no data to `glTexImage2D`, so a new texture's contents
  are undefined; the dummy texture has none.
- **Row order.** The glyph atlas treats its first uploaded row as the top,
  avg's y axis pointing up (`clipQuad` in `rendering.cpp`).
- **Opacity tests** for outlines: `isOpaque` in
  `src/bqui/src/shape/shape.cpp` and `isOpaqueColour` in `surfacenode.cpp`.
- **Repaints.** bqui has no public frame request. A window repaints when
  its ase window has a frame requested and the loop ticks: `WindowBridge`
  does both from its signal wakeups (`src/bqui/src/windowbridge.cpp`), and
  `onFrame` then paints its current drawing even when the tree did not
  change. `AppDeferred::wakeLoop` (`src/bqui/src/app.cpp`) only wakes the
  loop; `GenericWindow::requestFrame` writes a plain `std::optional`.
- **That write races today.** `GenericWindow::requestFrame` sets
  `nextFrame_` unguarded (`src/ase/src/genericwindow.cpp`), which the loop
  reads in its tick (`src/ase/src/platformbase.cpp`). The bridge's wakeup
  calls it synchronously, and `SignalContext::observe` runs a wakeup on
  the thread that made the change
  (`src/bq/include/bq/signal/signalcontext.h`), so a worker setting an
  input a window reads already races the loop. The platform's wake that
  follows is safe from any thread: an atomic flag and `RunLoop::post`.
  Dummy and offscreen windows skip it and only record the request.
- **The application's window.** `bqui::Window`
  (`src/bqui/include/bqui/window.h`) shares a `WindowData` holding the
  title, the close callbacks and a weak reference to the App
  (`src/bqui/src/windowdata.h`). The `WindowBridge` and its ase window
  live in the App's private list (`src/bqui/src/app.cpp`), so a `Window`
  cannot reach a frame request today.
- **One render context per run**, made in `App::run`
  (`src/bqui/src/app.cpp`) and shared by the windows; bqui exposes it to
  no one.

## The image

`avg::Image` is a cheap-to-copy handle to one shared state holding:

- its **texture**, an `ase::Texture` of format `FORMAT_SRGBA`;
- its **size** in pixels;
- its **scale**: pixels per logical unit, 1 by default, 2 for an asset
  drawn at twice the density. The natural logical size is size / scale;
- its **row order**: top row first (uploaded data, as the glyph atlas) or
  bottom row first (a texture rendered with clip-space +y up);
- an **opaque** declaration, made by the application: it promises every
  pixel it will ever write has alpha 1. False by default.
- a **pending upload**, empty or the latest pixels waiting for the painter
  (next sections).

All but the pending upload and the pixels are fixed for the image's life.
Equality, hashing and ordering follow the texture handle: two images
wrapping the same `ase::Texture` are equal, two images made from identical
bytes are not. An image can be a signal value, a `std::optional`, a member
of a theme; there is no empty image, "no image" is `std::optional<Image>`.

## Making an image and its pixels

The toolkit decodes nothing: reading PNG, JPEG or any file is the
application's job, with whatever library it prefers.

- **A new image** is made from a render context, a size and the fixed
  properties. bqui gains an `App::renderContext()` accessor, valid on the
  loop thread while the App runs (the render view needs it too). The
  texture is made at once, so the handle has its identity from the start.
- **Wrapping a texture** the application made on that context: it states
  the row order and the opaque declaration; avg trusts both.

Pixels arrive in two ways:

- **Uploaded.** `upload` takes tightly packed 8-bit sRGB RGBA rows, top
  row first, with **straight** alpha (what decoders produce), for the
  whole image. It converts them to the stored form on the calling thread
  and stores them as the pending upload, under a mutex, replacing any
  earlier pending one: the latest wins. It may be called from **any
  thread**. The painter takes the pending upload the next time it lowers
  the image, on the loop thread, and pushes it into the frame's command
  buffer ahead of the draws, as glyph uploads are; one window's painter
  takes it and every window then samples the same texture.
- **Rendered.** The application draws into the texture with ase on the
  **loop thread**, outside the frame, submitting to the context's main
  render queue, so the work is ordered before the next frame's draws.
  Rendering on another thread or inside the frame is the render view's
  business, not this note's.

**A new image is transparent black.** ase leaves a new texture undefined,
so avg pushes a zero upload when it makes one; until the application's
pixels arrive the image draws nothing, never garbage. A wrapped texture is
taken as it is.

## Asking for a repaint

Since changing pixels changes no value the UI reads, no signal wakes a
window. Today an application can only force a frame by changing some
signal a window reads, which repaints by accident. Frame cadence is per
window, so the request belongs to the window: bqui's `Window` gains a
**frame request** (a working name), callable from any thread, that marks
that window due and wakes the loop; the window then repaints its
unchanged drawing, which samples the new pixels. An image shown in
several windows needs a request to each. The application knows where it
put the image; a window it does not ask keeps the old pixels until it
next repaints for another reason. A request to a window in no app, or
one already closed, does nothing.

bqui must add a route from the `Window` handle to its bridge, through the
`WindowData` it shares with the App, set when the bridge is made and
cleared when it goes, under that object's mutex. The ase window's request
behind it is the racy write above, so the same stage makes it safe off
the loop thread: posted to the run loop, as the platform's wake already
is, or an atomic due time. That also fixes the race from signal wakeups.
On dummy and offscreen windows the request must also wake the platform.
The typical sequence is `image.upload(pixels)`, then a frame request to
each window showing the image, from a worker thread.

## Pixel contract

- **Format.** `FORMAT_SRGBA` only. Wrapping a texture of another format
  asserts in debug when the image is made and draws nothing in release.
  `FORMAT_R8` masks and `FORMAT_RGBA16F` are non-goals.
- **Colour.** The stored bytes are the sRGB encoding of linear,
  **premultiplied** colour: exactly what an avg render target holds after
  drawing. Sampling decodes to linear and the result blends `One,
  OneMinusSrcAlpha` like every other element. Premultiplied storage also
  keeps linear filtering free of dark fringes at transparent edges.
- **Conversion of uploads**: decode each channel to linear, multiply by
  alpha, encode again. Opaque and fully transparent pixels come out exact;
  dark translucent ones lose a little precision, accepted for SDR.
- **Rendered pixels** must already follow the contract; rendering with
  avg's blending does. With `GL_FRAMEBUFFER_SRGB` on, a producer writes
  linear values and must not encode them itself.
- **Filtering** is the texture's own: linear, no mipmaps, so strong
  minification aliases until ase has sampler state and mip generation.

## The image brush

`Brush` gains a third kind: an **image**, a **mapping** (an `Obb` in the
element's space onto which the whole image is mapped, possibly unset), a
**fit** with an **alignment**, and an **opaque** flag that defaults to the
image's declaration.

- **Mapping resolution.** A brush with no mapping gets one when its
  `ShapeElement` is made, from the bounds of the shape it fills and the
  fit (next section). One helper does it, called on every path that makes
  an element from a new brush: the three `Shape` functions, the two
  elements in `SurfaceNode::draw` and the one in `builtineffects.cpp`. The
  rebuilds in `withResource` and the text fallback copy a brush that is
  already resolved (or, for text, unsupported) and need no call. Lowering
  asserts in debug that an image brush has a mapping.
- **Transforms compose into the mapping.** `operator*(Transform, Brush)`
  and the float scale of `Drawing::operator*` map the mapping, instead of
  passing only a scale on, so the image follows the shape through every
  later transform, rotation included. A brush still unset stays unset.
  The image follows transforms of the drawing, not those baked into a
  shape before it is filled: filling a rotated shape maps the image onto
  its axis-aligned bounds.
- **Equality.** Same image, mapping, fit, alignment and flag. New pixels
  in the image leave the brush equal, so the tree does not change. Two
  image brushes are `isSameKind` when they share the image, fit and
  alignment, and `lerp` interpolates their mappings; any other pair
  switches at once, as `isInstantLerp` already arranges for brushes of
  different kinds.
- **The other sites** that read a non-effect brush as a colour gain the
  image case: `getColor`, ordering (colours, then images by texture, then
  effects), printing and float scale in `brush.cpp`, `hash_append` in
  `brush.h`, `lerp.cpp`, `Painter::getPipeline` and the vertex makers in
  `rendering.cpp`.
- **Lowering.** The fill is tessellated as for a colour. Each vertex gets
  a uv from its position in the shape's space, before the mesh's
  transform, through the inverse of the mapping, then the row order, so
  clipping, rotation and stretching stay exact. Where the shape reaches
  outside the mapping (*contain*), clamp-to-edge would smear the border
  pixels, so the fill shape is first intersected with the mapping's
  rectangle; this is skipped when the mapping covers the shape's bounds,
  the common case. Vertex colour is premultiplied white.
- **Two pipelines** follow `makeTextPipeline`: the same target and depth
  formats, the text vertex layout (colour, position, uv) and one sampler.
  The opaque one has no blend and writes alpha 1, so it sorts with solid
  colours; the translucent one blends and keeps paint order. Image draws
  batch only with draws of the same texture, which `renderElements`
  already enforces. Effect sub-drawings draw into scratch targets of the
  same formats, so they use the same pipelines.
- **Fills only.** An image brush on a pen, a `TextEntry` or a `RegionFill`
  draws nothing and asserts in debug.
- **Free from existing machinery:** rectangular clipping (`ClipElement`),
  any shape as the fill geometry, backdrop scopes snapshotting the result,
  and glass over an image.

## Fit

Fit decides the mapping from the shape's bounds `B` and the image's
natural logical size `S`, when the mapping is resolved:

- **fill**: the mapping is `B`; the image stretches.
- **contain**: the largest rectangle of `S`'s aspect inside `B`, placed by
  the alignment; the rest of the shape stays empty.
- **cover**: the smallest rectangle of `S`'s aspect containing `B`,
  placed by the alignment; the overhang is cut by the shape.

Alignment is a point in the unit square, centre by default. An explicit
mapping bypasses fit. Since size and scale are fixed, new pixels never
move a resolved mapping.

Decided here: the **brush defaults to fill**, the one rule that needs no
aspect and the one the render view wants; the **widget defaults to
contain**, which never distorts or crops a picture the layout resizes.

## Opacity and outlines

`isOpaque` and `isOpaqueColour` decide whether a fill adds an outline,
which drives shadows. An image fill follows its opaque flag: an opaque
image adds an outline along its fill and casts a shadow like a solid
colour; a translucent one does not, so an icon with a transparent border
casts no box. Under *contain* an opaque brush still leaves part of the
shape empty, so it adds no outline unless the mapping covers the shape's
bounds. The flag comes from the declaration, not the pixels, because the
pixels change without the tree knowing.

## Texture lifetime

The image's shared state owns the texture, so the texture lives while any
copy of the image does: in a brush, a `Drawing`, a render tree node
(including a leaving one in its exit transition) or a signal. A draw
command in flight holds its own `ase::Texture` copies
(`CommandBuffer::push`), so dropping the image mid-frame is safe.

The last copy can drop on any thread: usually the loop thread, a worker
thread when a decoding task's signal value is replaced. Verified in ase:
`GlTexture::~GlTexture` calls `destroy`, which hands `glDeleteTextures` to
`GlRenderContext::dispatchBg` (`src/ase/src/gl/gltexture.cpp`); that queues
the closure on the context's transfer queue, whose `Dispatcher::run` locks
a mutex (`src/ase/src/dispatcher.cpp`) and runs it on the dispatcher's own
GL thread. Destruction from any thread is therefore safe. The one hazard:
`GlTexture` holds a plain `GlRenderContext&` (`gltexture.h`), so an image
must not outlive the App's render context; one kept in a static or a
signal past `App::run` dangles. The dummy texture releases nothing.

## bqui: the image widget and brushes on shapes

- **Any shape.** A shape widget's `fill` already takes a brush or a
  signal of brushes (`src/bqui/include/bqui/shape/shape.h`), so
  `shape::circle().fill(Brush(avatar, Fit::cover))` is an avatar and needs
  no new bqui code beyond the outline rule above.
- **`widget::image(image, fit)`**: a rectangle filled with an image brush,
  whose natural size is the image's natural logical size, so an icon made
  at scale 2 is laid out at half its pixel size. The image may be a signal;
  a new image re-lays out only when its natural size changes. Fit defaults
  to contain, alignment to centre. A rounded image is a shape fill, not an
  option of the widget.
- **`App::renderContext()`** and the **window's frame request**, above.

Only the widget and modifier surface aims for stability; `avg::Image`,
the brush layout and the painter internals may change.

## Relation to the render view

The render view builds on this brush: its views are image fills whose
pixels come from an engine. Whether it adds a mutable slot in the drawing
or keeps the mutable state in the render tree, leaving the brush a plain
value as designed here, is being decided separately; this note does not
design it, and nothing here depends on the answer.

## Testing

No pixel readback exists on this stack, so most tests check what the
painter submits on the dummy backend, and pixels are checked by hand.

- **Values (dummy, CI).** `src/avg/test/brushtest.cpp`: equality, ordering,
  hash, printing, `isSameKind` and `lerp` of image brushes, and equality
  unchanged by an upload; the conversion (opaque and transparent pixels
  exact). `src/avg/test/drawtest.cpp` and `shapetest.cpp`: mapping
  resolution on every element path, each fit and alignment, composition
  under translate, rotate and float scale.
- **Lowering (dummy, CI).** `src/avg/test/renderingtest.cpp` renders a
  `Drawing` and inspects the commands, as `clipRectCutsQuadsAndTheirTexels`
  does for glyph quads: uv under clipping, rotation, stretching and both row
  orders; the contain intersection; the bound texture and pipeline; opaque
  images sorted first; the zero upload at creation; a pending upload pushed
  once, before the draws, the latest of several; a wrong format drawing
  nothing. `effectrenderingtest.cpp`: an image in an effect sub-drawing and
  under glass.
- **bqui (dummy, CI).** `src/bqui/test/shapetest.cpp` and
  `shadowboundarytest.cpp`: the outline under opaque, translucent and
  contain fills; `widgettest.cpp`: the natural size from size and scale;
  `apptest.cpp`: a window's frame request from another thread repaints
  that window, and only it, with an unchanged tree, with
  `DummyRenderQueue` recording the submissions.
- **Pixels (real GL, manual).** sRGB round trip, premultiplied edges under
  filtering, row order, transparent black before an upload and alpha 1
  from the opaque pipeline need readback: `bquiheadlesstest`
  (`src/bqui/meson.build`) renders offscreen by hand, and the unmerged
  `origin/remote-screenshot` branch's `window.screenshot` reads pixels back
  once it lands; a known image must read back exactly at texel centres.

## Dependencies on unmerged work

- **The effect-brushes stack** (`origin/effect-brushes`): the variant
  `Brush`, painter, pipeline API and nested `Drawing`. Build on it once it
  is merged.
- **remote-screenshot**, for readback tests only.

## Staged plan

Each stage is one PR, after the effect-brushes stack is merged.

1. **avg: the image and the brush.** `Image` with both ways of making it,
   `upload` with its conversion and pending upload; the brush kind with
   mapping, fit and alignment, the resolution helper on every element
   path, transform and float-scale composition, and every brush site
   except lowering; the opacity tests in `surfacenode.cpp` and bqui's
   `shape.cpp`. Value and drawing tests.
2. **avg: drawing it.** The two pipelines, lowering with per-vertex uv and
   the contain intersection, the zero upload, taking the pending upload,
   the format check. Dummy command tests; one manual readback.
3. **bqui: the widget.** `widget::image` with fit and alignment, natural
   size from the image's scale, `App::renderContext()`, and the frame
   request on `Window`, with the ase window's request made safe off the
   loop thread. Headless tests.

## Open questions for the owner

Each carries a recommendation as a working default.

1. **Images before `App::run`.** The context exists only while the App
   runs, so an image cannot be made earlier, for a theme built up front.
   *Recommendation:* accept it for now; lift it when the render
   architecture gives the App a context before `run`.
2. **Natural size.** From the image's own scale (proposed), or one image
   pixel per device pixel, which makes layout depend on the display.
   *Recommendation:* the image's scale.
3. **Premultiplied uploads.** Straight alpha only (proposed), or also
   premultiplied bytes, skipping the conversion. *Recommendation:* straight
   only until someone has premultiplied data.

## Non-goals

- Decoding image files; loading by path or URL; asynchronous loading
  beyond a worker thread calling `upload`.
- Partial uploads, HDR, wide gamut, `FORMAT_R8` masks, mipmaps, sampler
  state, tiling and nine-patch images.
- Image brushes on pens, text and region fills; tinting and group opacity.
- Rendering into an image off the loop thread or inside the frame, and
  any per-frame engine callback: the render view.
