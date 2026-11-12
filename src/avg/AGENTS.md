# avg — agent notes

*Last verified against `44d4091` (2026-07-22); the text sections against
`f3ed6e5` (2026-10-22); the painter section against `3c3dfb34` (2026-11-12);
the shapes-and-drawings and render tree sections against `8220ce96`
(2026-11-12); the effects section, and the effect notes in those two
sections, against `f914b404` (2026-11-12).*

Internals, entry points, and traps for the vector-graphics layer. Concepts and
usage are in `readme.md`; project-wide conventions are in the top-level `docs/`.
This file is the index for `avg`; split topics into a `.agent/` folder here if it
outgrows one file.

## Shapes and drawings

- `Shape` (`shape.h`) is geometry: a tree of `Path`/boolean-op/stroke elements
  with per-element transforms. Boolean ops are `intersect`/`add`/`subtract`;
  there is **no `operator&`** (use `intersect`). It can rasterise to a `Region`
  via `fillToRegion`/`strokeToRegion`.
- `ShapeFunction` (`shapefunction.h`) is a time-parameterised shape:
  `(time, DrawContext, size) -> Shape`, wired into the animation system. This is
  what `bqui`'s shape builder wraps.
- `DrawContext` (`drawcontext.h`) is the memory a draw allocates out of, and
  nothing else. It takes either a `Painter*` or a bare `pmr::memory_resource*`,
  so a tree can be drawn and its `Drawing` asserted with no rendering back end.
- `Drawing` (`drawing.h`) is the flat rasterisable output — a variant of
  `ShapeElement` / `TextEntry` / `ClipElement` / `RegionFill` /
  `BackdropScope` / `EffectElement`, painted in list order. **Clipping is
  rectangular only**: `Drawing::clip` takes `Rect`/`Obb`, and there is no
  region/shape clip. (Path-level clipping is deferred — `docs/decisions.md`.)
- **Pending shadows.** A `Drawing` carries a second element list, the pending
  shadows: elements that belong *under* the content of whatever container ends
  up holding the drawing. `+=` concatenates both lists, `clip`/`transform`/
  `filterByRect` apply to both, `getControlBb` covers both, and `flush()` moves
  the shadows ahead of the main list. `RenderTree::draw` flushes the root
  drawing; anything that renders a node's drawing directly (tests, snapshot
  paths) must flush it first, or the shadows are silently dropped (`render`
  only sees the main list).
- **`BackdropScope`** is a sub-drawing with its own transform, shaped like
  `ClipElement` without the clip; `scope()` wraps the main list into one, and
  `over(below, above)` is the one composition for painting a child over
  another: below's main list, above's pending shadows, then above's main list
  in a scope (below's shadows stay pending). Scopes nest and `+=` never
  flattens them. A scope caches its contents' bounds (`controlBb`, in scope
  space, strokes included) so bounds queries do not recurse. The renderer
  (`rendering.cpp`, next to the `ClipElement` case) recurses into a scope
  with the transform composed and the culling rect mapped by the inverse
  transform's bounding rect, changes no clip, and opens a scope record that
  the effect elements directly inside mark used; only a used scope snapshots
  (see *Effects*). **A rotated scope cannot sit under a clip**: the clip
  rect would map to a larger bounding rect in scope space and content
  outside the clip would show, so the renderer asserts, as it does for a
  rotated `ClipElement`. The snapshot text collector
  (`rendertree/snapshot.cpp`) recurses into scopes like clips.
- **`EffectElement`** is a `Shape`, a `std::shared_ptr<Effect const>` and a
  `Transform`: the effect's final pass paints the shape, the passes before
  it sample the enclosing scope's backdrop around it. The shape stays in
  the element's own space and `transform`, `translate` and `operator*`
  compose into the element's transform as they do for a scope, so the
  renderer still sees the element's own scale. The `Effect` is shared,
  never copied:
  one effect keeps its identity across elements and frames, which is what
  the emitter's caches key on. Bounds (`getControlBb`, `filterByRect`, the
  renderer's culling) are the transformed shape bounds with no padding;
  the margin the passes sample is derived at render time
  (`deriveRegions`). `clip` wraps it like any element: the renderer
  intersects the element's bounds with the clip rect before deriving the
  regions, so the final pass's scissor is the clipped bounds and the
  passes sample no further than they need. The renderer turns the element
  into an effect draw in device pixels (*Effects*); the snapshot text
  collector ignores it.
- `btl/optional.h` prints `std::optional` through an `operator<<` at
  global scope, which unqualified lookup only reaches when no
  `avg::operator<<` is declared first. `effect.h` declares several, so
  `TextEntry`'s printer (`textentry.h`) names the global one with
  `using ::operator<<`; keep that when touching it, or the include order of
  a test decides whether `drawing.h` compiles.
- `Shape::getControlBb` composes each sub-element's transform once and bounds
  an `intersect` by its first operand (`shape.cpp`, `getControlBb`); the
  `Shape` tests pin this.

## Render tree

- `rendertree.h`: an **immutable** tree of `RenderTreeNode`s. Node kinds:
  `ContainerNode` (matches children by id; diffs insert/remove/reorder),
  `ShapeNode`/`RectNode` (animated leaves), `TransitionNode` (enter/leave),
  `ClipNode` (clips a child to an `Animated<Obb>` — **rectangle only**),
  `IdNode` (stable identity for matching), `SurfaceNode` (a filled shape with
  a shadow and an optional backdrop effect under a child subtree).
- `oldTree.update(newTree, options, time)` splices trees (creating animated
  transitions); `draw(context, obb, time)` is a **pure** function of the
  timestamp and returns whether animation continues, so nodes report when they
  next need a redraw. `RenderTree::draw` is the root: it flushes the drawing
  (pending shadows ahead of the content) before returning it.
- **Stacking.** `ContainerNode` takes a `Stacking` flag. `flat` (the default,
  what vbox/hbox/grid use) concatenates the children's drawings with `+=` and
  passes their pending shadows up unchanged, so nested flat containers draw
  the same elements as one flat container. `stacked` paints the first child
  flat and each later child with `Drawing::over`, i.e. its pending shadows
  then its content in a `BackdropScope`, so a child's backdrop effects sample
  the children below it and their shadows but nothing of itself, and a nested
  stack's scopes stay inside its own scope. `update` keeps the new tree's
  flag; the snapshot still reports a stacked container as `ContainerNode`.
- **`SurfaceNode`** (`rendertree/surfacenode.h`) holds a `ShapeFunction`
  (the shape for the node's size, animated like a `ShapeNode`'s parameters),
  an `Animated<std::optional<Brush>>` fill, an optional `SurfaceShadow`
  (animated offset, spread and brush), an optional `SurfaceEffect` and a
  child. `draw` adds the shadow as a **pending shadow** (the shape translated
  by the offset, filled with the shadow brush and, for a positive spread,
  stroked with a pen of width `2 * spread` in the same brush), then the
  effect element, the fill and the child's drawing in the main list, in that
  order. The node's own animation options drive its obb, shape, fill, shadow
  and effect only; like every other node it hands the inherited options to
  its child. It never adds a scope: that would give every card in a vbox a
  private backdrop.
- **`SurfaceEffect`** is `std::variant<SurfaceFrostedGlass, SurfaceTint>`,
  each alternative holding `Animated` members (`Animated<FrostedGlassOptions>`;
  `Animated<Color>` plus `Animated<float>` strength), so effect parameters
  animate as values and never change the drawing's structure. `draw` turns
  the members' values at the frame's time into an `Effect` through
  `frostedGlass`/`tint` and wraps it in a fresh `shared_ptr` for the
  `EffectElement`; `update` interpolates member by member when both trees
  hold the same alternative and takes the new value otherwise. A curve
  that overshoots (`easeOutBack`) carries interpolated values out of the
  range the built-ins accept, so `makeEffect` (`surfacenode.cpp`) clamps
  strengths to [0, 1] and the radius, saturation and highlight width to
  non-negative before building; the built-ins themselves still throw on
  such input. Rebuilding
  the `Effect` every frame is deliberate for now: the shader source does
  not depend on parameter values, so an emitter cache keyed on source is
  unaffected, and only the parameter bytes change; caching the `Effect` per
  node while the options are unchanged is a later optimisation.
  `FrostedGlassOptions` has `==` and a memberwise `lerp` (`builtineffects.h`)
  to satisfy `Animated`; like `Animated<std::optional<Brush>>` it is
  instantiated implicitly, not in `animated.cpp`.

## Render tree snapshots

`rendertree/snapshot.h` describes a tree instead of drawing it, for tooling
outside the process. `RenderTree::snapshot` mirrors `draw`: same arguments,
same pure-function contract, same subtree — a transition contributes only the
branch that is on screen. Each node describes itself through the `snapshot`
virtual, so adding a node kind means adding that override.

- **Geometry is resolved, not authored.** A node reports its box already
  composed with every enclosing obb, so a reader places it without a transform
  of its own. It is derived from the nodes' `Animated<Obb>`, never from
  `Shape::getControlBb`.
- **Text comes from drawing, not from the tree.** A leaf's content lives inside
  its draw function, so a leaf is drawn on its own out of the caller's memory
  and the `TextEntry`s of the result are collected; the drawing is discarded.
  A snapshot therefore costs a whole draw pass. Because a leaf is drawn in
  isolation, an enclosing `ClipNode` applies itself afterwards, through
  `clipSnapshotText`.
- **Nulls are expected.** An empty tree yields no root, and a node whose child
  is null yields no child; the walker never assumes a well-formed tree.

A `Snapshot` is pure data — node kinds, ids, resolved boxes and the text the
leaves draw. `avg` deliberately does not serialise it: JSON is a `bqui` concern
(it owns the `nlohmann` dependency and the remote wire format), so the schema
lives in `bqui`'s remote layer, not here. Keep `Snapshot` free of any
serialisation coupling.

## Effects

`effect.h` holds the value model of a backdrop effect, `effectplan.h` the
pure planning over it, `effectshader.h` the shader contract every pass is
written against and `builtineffects.h` the effects avg ships; none of them
touches `ase` beyond naming a `Format`, a `ShaderInterface` and
`UniformValues`. `scratchpool.h`, `passpipelinecache.h` and
`effectemitter.h` are the GPU-facing half: they turn a planned effect into
`ase` commands. A `Drawing::EffectElement` places an effect in a frame
(see *Shapes and drawings*) and `SurfaceNode` animates one (see *Render
tree*); `rendering.cpp` runs the scope pre-pass that snapshots backdrops
and drives the emitter, with the pool and the cache owned by the `Painter`
(the last bullets below). `docs/design/backdrop-effects.md` is the design,
with the rejected alternatives and the places the code departed from it;
`docs/decisions.md` records the decisions.

- An `Effect` is a list of passes in execution order, so it is a DAG by
  construction: a pass names its inputs as the backdrop (the enclosing
  scope's snapshot) or an earlier pass, its output target as a divisor of
  the screen size (1, 2, 4 or 8) plus a format, and a padding, in pixels of
  its own output, for how far outside its region it samples. The last pass,
  and only the last, draws the shape into the main target and has no
  output. Construction validates and throws listing every broken rule
  (`validate` returns the same list); an output nobody reads is an error, so
  the planner never sees dead work.
- **Regions are derived backwards.** `deriveRegions` starts from the
  shape's screen bounds and walks to the first pass: a pass writes the union
  of what its readers sample, each grown by that reader's padding and
  rescaled to the pass's divisor with the origin floored and the far edge
  ceiled, then clamped to the pass's own target (`targetSize`, the screen
  size divided and rounded up). The union of the backdrop reads, at full
  size, is what the scope's snapshot copy must cover.
- **Scratch is allocated by liveness over one timeline.** A
  `ScratchTimeline` lists a frame's scope snapshots and effects in order,
  each effect naming the snapshot it reads. `planScratch` numbers every
  snapshot and pass as a step, finds each value's last reader, and runs a
  linear scan per (divisor, format) pool: a pass takes its output texture
  *before* its inputs are released, so an output never aliases an input of
  the same pass (the GL feedback-loop rule holds without a barrier), the
  lowest free texture is reused first, and a value nobody reads is released
  at its own step. A chain therefore ping-pongs between two textures, a
  wide graph takes one per live value, consecutive effects reuse the same
  textures, and a snapshot stays live until its last reading pass. The plan
  reports per pass the texture of its output and of each input, per value
  its definition and release steps, and per pool the texture count.
- **Every pass shader shares one contract** (`effectshader.h`,
  `effectshader.cpp`). The vertex stage is one GLSL 330 shader taking `pos`,
  a vec2 in the pass's target pixels, and a fragment shader is the prelude
  `passFragmentPrelude` emits plus a body. The prelude and `passInterface`
  are generated from the same lists, so the GLSL and the declaration cannot
  drift: attribute `pos` at location 0; the std140 block `PassBlock` at
  binding 0 with a fixed layout (`targetSize`, `targetDivisor`, `region`,
  `texel0..texel3`, 96 bytes) that the renderer fills per draw and the
  vertex shader declares identically so every program links the same
  block; `ParamBlock` at binding 1 holding the effect's own parameters, left
  out when there are none; one `sampler2D` per input, `input0..`, at the
  unit of the input's index, so the draw's positional texture list is the
  binding (the ase rule). At most four inputs (`maxPassInputs`); `validate`
  rejects a pass with more, or whose shader does not declare exactly one
  sampler per input. The GLSL carries no
  `layout(binding)` and no 420pack, as `ase` applies the declaration at
  link.
- **Fragments locate themselves through `gl_FragCoord`, not a varying.**
  `screenPos()` is the fragment centre times the pass's divisor, in screen
  pixels; `inputUv(texel)` divides it by the input's size in screen pixels.
  With equal divisors a fragment hits its texel's centre; with a finer
  input it hits the corner shared by the middle 2x2 input texels of the
  block it covers, and a plain bilinear fetch averages those four. That is
  a box downsample when the input is twice as fine. When it is four or
  eight times as fine the fetch reads 4 of the block's 16 or 64 texels and
  skips the rest, so detail finer than the block aliases. Either way the
  copy pass needs no padding.
  Because nothing but `gl_FragCoord` and `PassBlock` place a fragment, the
  fragment shaders work under any vertex stage the renderer uses for the
  final Shape pass (the mesh path with depth, for instance).
- **Parameters travel as bytes.** `Pass::parameters` is an
  `ase::UniformValues` of the shader's `ParamBlock`, filled when the effect
  is built and uploaded as it is for every draw; `validate` checks that the
  block it mirrors is the one the shader declares. This keeps the shader
  source constant across parameter values, so an animated radius or tint
  does not defeat a pipeline cache keyed on the source.
- **`frostedGlass`** (`builtineffects.cpp`) is four passes, all scratch in
  `FORMAT_RGBA16F` at one divisor: a copy of the backdrop (padding 0, the
  single bilinear fetch above: a box downsample at divisor 2, the middle
  2x2 of each block at 4 and 8), a horizontal and a vertical Gaussian
  (padding = tap radius = ceil(radius / divisor), sigma = half the radius
  in texels, floored at 0.5 so a zero radius divides by something), and the
  Shape pass reading only the blurred result (padding divisor / 2, half a
  blurred texel, for the bilinear upsample). The divisor is the smallest of
  2, 4, 8 at which the per-pass radius stays within 16 texels
  (`frostedGlassDivisor`), so a blur pass is at most 33 taps up to radius
  128 screen pixels; beyond that the tap count grows at divisor 8. The
  final pass does not read the backdrop, so the snapshot is released after
  the copy pass instead of living to the end of the effect. Its look is
  saturation scale, tint by strength (the alpha of the tint and highlight
  colours is ignored) and an edge highlight blended in over a width
  measured from the pass's *region* (the shape's screen bounds, clamped):
  an approximation that an SDF would replace, kept as the
  parameter slot for one.
- **`tint`** is one Shape pass reading the backdrop and mixing a colour in
  by strength times the colour's alpha; result alpha is always 1, as the
  final pass is opaque by design.
- **Scratch textures live in a `ScratchPool`** (`scratchpool.h`), one per
  window: a pool per `ScratchKey`, every texture full-screen at its key's
  divisor (`targetSize`), each in a framebuffer of its own with no depth
  attachment. `acquire(slot)` makes the slot's texture on first use, so a
  frame allocates exactly what its `ScratchPlan` asks for; textures persist
  across frames, `beginFrame(screenSize)` with a new size drops them all,
  and `endFrame` drops a texture no frame has acquired for
  `scratchIdleFrameLimit` (120) frames, so a window that stops showing
  effects gives the memory back. Apart from a resize, the idle release is
  the only way the pool shrinks.
- **Pipelines are cached per (fragment source, target format, geometry)**
  in `PassPipelineCache` (`passpipelinecache.h`). An entry links
  `passVertexSource` with the fragment source against the pass's declared
  interface, resolves the `PassBlock` and `ParamBlock` bindings and the
  sampler count once, and draws with depth test and write off; a Region
  pipeline does not blend (the quad overwrites its region), the Shape
  pipeline blends source alpha over the main target. Parameters are not in
  the key, so an animated radius keeps its pipelines. Each entry owns a
  small pool of `UniformSet`s handed out per draw and recycled by
  `beginFrame`: a set is read when its draw executes, so two draws of one
  pipeline in a frame sharing a set would both see the last ranges bound.
  This differs from the painter's single `MatrixBlock` set, which is
  constant over a frame.
- **`EffectEmitter`** (`effectemitter.h`) records a frame's effect draws
  into the painter's command buffer. It is built per frame over the
  context, the command buffer, the `UniformRing`, the pool, the cache and
  the main `TargetImage`, whose size is the screen size the regions and
  plan were derived for. `emitSnapshot(slot, region)` is one quad draw with
  the copy shader, sampling the main target's texture into the divisor-1
  slot, scissored to the scope's backdrop union, between two barriers.
  `emitEffect(effect, regions, allocation, triangles)` then, per pass in
  order: acquires the input textures the allocation names (a backdrop input
  is the snapshot's slot, resolved by `planScratch`), fills `PassBlock`
  (`targetSize` at the pass divisor, `targetDivisor`, `region`, one
  `texel` per input with `1/size` and the input's divisor, zero beyond),
  writes it and the pass's `ParamBlock` bytes into the ring, binds them by
  number on a fresh uniform set, uploads a vertex buffer (six `pos` vertices
  for a Region quad; the caller's pixel-space triangle list for the Shape
  pass), pushes the draw with the pass's textures in input order and a
  scissor on the region (the Shape draw too, so stray triangles never leave
  the derived bounds), and ends with a barrier. A barrier is skipped when
  the buffer already ends in one, so a snapshot followed by its effect is
  `barrier, copy, barrier, pass, barrier, ...`. No pass clears: a Region
  quad overwrites its whole region and the Shape pass draws over the main
  target. An effect whose Shape region is empty records nothing; a pass
  with an empty region is skipped. `getPassCount` and `getSnapshotCount`
  count the pass and snapshot draws the emitter has recorded.
- **PassBlock in C++** is `avg::PassBlock`, a plain struct of float arrays
  whose std140 placement `ase::UniformBlock` checks against `passInterface`
  when the emitter is constructed, so a layout drift fails at the first
  frame, not on the GPU.
- **The scope pre-pass is the mesh pass.** `rendering.cpp` flattens a
  drawing once, in paint order, into items that are meshes, text quads, a
  `ScopeEntry` or an `EffectDraw`. `render` opens the root scope (the
  implicit one every effect outside a `BackdropScope` reads) and puts its
  entry first; the `BackdropScope` visitor appends a scope record, emits
  its entry and recurses with that scope as the current one, while a
  `ClipElement` keeps its parent's. The `EffectElement` visitor maps the
  element's bounds (intersected with the clip rect when inside a clip)
  through `toRoot` and the scaling factor to a `PixelRect`, clamps it to
  the target and drops the element when nothing is left; otherwise it
  fills the shape to a region at the element's point size, triangulates
  it into a device-pixel triangle list and drops the element when the
  list is empty (a shape without area), so such an effect costs no
  snapshot. Only then does it derive the regions, mark the current scope
  used and union the effect's `backdropRegion` into the scope's. Culling
  applies to scopes and effects as to any element, so a culled scope has
  no record and a culled effect uses none. The culling rect is the target
  in logical units (its pixel size over the scaling factor); the effect
  regions and `screenSize` are in pixels.
- **A clip cuts an effect with a device-pixel scissor only.** The clip
  rect shrinks the bounds the regions are derived from, and every pass
  draw is scissored to its region, but the final pass still draws the
  unclipped shape. The scissor is axis-aligned and snapped outward to
  whole pixels, so a clip with a fractional edge lets the effect through
  by up to a pixel, and a clip inside a rotated `BackdropScope` lets it
  fill the corners of the clip's bounding box.
- **Effect parameters are device pixels.** An `Effect` is built before any
  scaling factor is known and its divisor, paddings and parameter bytes
  are fixed at construction, so the renderer scales the shape and the
  regions but not a blur radius or highlight width.
- **Timeline, plan and emission.** After the walk, `render` builds the
  `ScratchTimeline` from the items in order (a snapshot per *used* scope
  entry, an effect per draw naming its scope's snapshot), and when it holds
  no effect draws everything as one batch: no emitter is made and the
  commands are those of a drawing without scopes. Otherwise `planScratch`
  runs once, `Painter::makeEffectEmitter` hands out the emitter over the
  frame's command buffer and target, and `drawItems` walks the items:
  meshes gather into a segment; a used scope entry and an effect draw each
  **flush the segment into draws first** and then call `emitSnapshot` with
  the scope's union, or `emitEffect` with the next `plan.effects` entry.
  The emitter's barriers bracket every one of its draws, so the flushed
  content is a run of its own before the snapshot and the next segment a
  run after the final pass: the snapshot sees exactly what paint order put
  below the scope, and the final pass lies between the content below and
  above it. Z is still assigned over the whole frame (`generateElements`
  takes the segment's first index and the frame's mesh count), so the
  depth test orders opaque content across segments, and a frame
  with no effects is one segment with the same z values and no barrier.
  This was chosen over giving the final pass a z: the Shape pipeline
  neither tests nor writes depth, so z would not order it against opaque
  content anyway, and a run boundary costs nothing when no effect is drawn.
- **The painter's share.** `Painter` owns the `ScratchPool` and the
  `PassPipelineCache` next to its pipelines. `beginFrame` begins the cache's
  frame with the uniform ring's; `makeEffectEmitter(commands, mainTarget)`
  opens the painter frame if needed, begins the pool's frame at the
  target's size (a no-op while the size holds) and builds the emitter over
  the ring, pool, cache and target; `flush` ends the pool's frame when one
  was begun. `render` therefore takes the `TargetImage` it draws into, not
  a framebuffer and a size: the window path passes `windowTarget_`, the
  image path the caller's image, and the emitter samples that target.
- `effectrenderingtest.cpp` pins the command structure of frames with
  effects on the dummy backend (clear, content, barrier, snapshot, passes,
  final pass, content; one snapshot per used scope, none for an unused
  one; the innermost scope's snapshot; the root snapshot; a clipped
  effect's regions; a `SurfaceNode` in a stack end to end), and that a
  drawing with unused scopes renders byte-for-byte like one without.
- Shader compilation and the look are the one thing these tests cannot
  reach: the dummy backend compiles nothing, so the GLSL is only checked
  textually (version line, declared names present, no binding qualifiers)
  until a real GL context runs it. `effectemittertest.cpp` checks the
  command structure on the dummy: framebuffer, textures, scissor and
  PassBlock bytes per draw, the barriers, pool reuse across frames and
  re-creation on resize, and pipeline sharing. Whether the blur looks
  right, whether sampling a texture written two draws earlier needs more
  than the FBO switch, and the cost of a pass on real hardware are manual
  WGL/GLX checks.

## Animation

- `Animated<T>` (`animated.h`): a value plus keyframes; evaluated by timestamp
  via `AnimatedTraits<T>`. `getValue(time)`, `updated(...)`, `hasAnimationEnded`.
- The `Animated<T>` explicit template instantiations live in
  `src/avg/src/animated.cpp` and are export-marked — if you add an animated type,
  add it there too (see `docs/conventions.md` on template export vs clang-cl).

## Painter

`Painter` (`painter.h`, `painter.cpp`) owns the pipelines, the glyph atlas,
the `UniformSet` every content draw binds, the window's offscreen target,
the effects' scratch pool and pipeline cache (see *Effects*) and the command
buffer the frame is recorded into. `clearImage`/`clearWindow`/
`paintToImage`/`paintToWindow` record; `flush` (and `presentWindow`, which
pushes the present quad and flushes first) submits.

- **Window frames always render offscreen.** `clearWindow` and
  `paintToWindow` name the framebuffer of `windowTarget_`, a `TargetImage`
  (`FORMAT_SRGBA` colour texture over a `FORMAT_DEPTH24_STENCIL8`
  renderbuffer, both attached in the `TargetImage` constructor) of the
  window's `getScaledSize()`. `Painter::windowTarget` makes it on first use
  and replaces it whenever that pixel size differs from the texture's, since
  nothing resizes in place; two frames at one size share the texture handle.
  `presentWindow` then pushes `pushBarrier()` and one draw of a static
  clip-space quad (uploaded once, `StaticDraw`) into the window's default
  framebuffer with the target texture at unit 0. The barrier is what keeps
  the run sort from moving the quad ahead of the draws it samples. The
  window is never cleared: the quad covers it and its pipeline neither tests
  nor writes depth. A frame that painted nothing still presents the target
  as it was; nothing is presented before the first clear or paint. Image
  frames (`clearImage`/`paintToImage`) draw straight into the given
  `TargetImage` and carry no quad. The painter therefore serves one window;
  a second window through the same painter would remake the target every
  frame.
- **sRGB round trip.** The target and the window are both sRGB surfaces
  under `GL_FRAMEBUFFER_SRGB`, so content blends linearly in the target, the
  present quad's `texture()` decodes, and the window write re-encodes. If a
  platform cannot give an sRGB window format, the present fragment shader
  (`presentFsSource`) is where a manual encode belongs; none is implemented.
  No GL context runs in CI, so the quad's pixels are a manual check
  (`lw run app`); `test/renderingtest.cpp` asserts the command shape on the
  dummy backend: clear of the target, draws naming it, barrier, one present
  draw naming the window with the target texture bound, and the target
  handle surviving a same-size frame and changing on a
  `DummyWindow::resize`.
- **The shader interfaces are declared in `painter.cpp`** (`matrixInterface`:
  `pos` at location 0, `color` at 1, `MatrixBlock` at binding 0; the text
  pipeline adds `uv` at 2 and the `atlas` sampler at unit 0;
  `presentInterface`: `pos` at 0, `uv` at 1, the `source` sampler at unit 0
  and no block, so the present draw binds an empty `UniformSet` and its
  vertices are already in clip space). The GLSL carries no binding
  qualifiers; `ase` applies them from the declaration and validates by
  reflection in debug builds, so a shader whose names drift from the
  declaration fails at program creation. Change the declaration and the
  shader together.
- **The projection is a typed block through a ring.** `setSize` only sets
  `MatrixBlock::worldViewProj` in a `UniformBlock`. The first clear or paint
  after a `flush` opens a frame: `UniformRing::beginFrame`, one `write` of
  the block, `bindUniformBufferRange("MatrixBlock", range)`; a `setSize`
  while a frame is open writes and rebinds. `flush` closes the frame with
  `endFrame` into a separate command buffer and submits that *before* the
  draw buffer, so the frame's uniform upload precedes every draw that binds
  into it, and no upload sits among the draws to fence their batching. Since
  the ring rotates buffers per frame and the set is rebound every frame, a
  frame never draws with a range from an earlier frame.
- `avg::render` (`rendering.cpp`) is the free function the painter records
  through, and tests call it directly with their own command buffer; it
  opens a painter frame only when the drawing has effects (through
  `makeEffectEmitter`), so a test that needs the uniform path drives the
  painter and reads the dummy queue's recorded submissions. It asserts that
  the drawing has no pending shadows: whoever renders a drawing flushes it.

## Vendored Clipper

`src/clipper/` is upstream Clipper, not ours. It builds as its own `libclipper`
static target with warnings off, so a file added there gets no diagnostics —
and a file added to `libavgsrc` instead gets all of them. Keep changes to it to
what upstream would recognise.

## Text

Text goes through `Font`/`FontManager` (`font.h`, `fontmanager.h`); FreeType
is the backend. Layout shapes text into a `Path` out of a caller-supplied
`pmr::memory_resource` — the same resource a `DrawContext` carries. Drawing
goes through a glyph atlas instead, and falls back to that path. Entry
points: `fontmanager.cpp` (`walkGlyphs`, `FontManager::rasterizeGlyph`, to
which `Font::rasterizeGlyph` forwards),
`src/atlasallocator.h` (`AtlasAllocator`, the packing), `src/glyphatlas.h`
(`GlyphAtlas`, the texture), `rendering.cpp` (`generateTextQuads`, the draw
path and its fallback rules) and `painter.cpp` (the text pipeline and its
shader).

- One walk over the text (`walkGlyphs` in `fontmanager.cpp`) drives the path,
  the extents, hit-testing and `Font::getGlyphs`; change layout there, not in
  the callers.
- Metrics and outlines are read at one fixed FreeType size (the "metric size",
  whose 26.6 coordinates are divided by 6400 to get the unscaled units).
  `Font::rasterizeGlyph` uses separate per-scale `FT_Size` objects kept on the
  `FontImpl`, and must leave the metric size active again before anything else
  touches the face — the glyph cache assumes it.
- **Nothing is hinted** (`FT_LOAD_NO_HINTING` for metrics, outlines and
  bitmaps alike), so layout metrics stay fractional and the atlas path and
  the outline path measure identically. `GlyphKey` (`atlasallocator.h`) is a
  struct of `font`, `glyphIndex`, `pixelSize` and `subpixelBin` rather than a
  packed integer so a hinting mode can be added as one more field without
  touching the allocator or the atlas.
- `Font::rasterizeGlyph` renders one glyph at an integer pixel size with its
  outline shifted by one of `subpixelBins` (4) fractions of a pixel in x.
  There are no y bins: `generateTextQuads` rounds the pen's y to the device
  pixel.
- **Fallback rules** live in `generateTextQuads`. A `TextEntry` is drawn as
  atlas quads only when it has a brush and no pen, neither it nor any
  enclosing transform rotates (which also covers a rotated clip, since the
  clip rectangle is taken in the enclosing space), and its rounded device
  pixel size is within 1..`maxAtlasPixelSize` (128). Any other entry, and an
  entry whose glyph the atlas cannot place this frame, goes through
  `textToPath` and the outline tessellation like a shape; nothing is ever
  rasterised through a transform. Each quad is clipped against the enclosing
  clip rectangle in device space.
- The text pipeline (`painter.cpp`) samples the atlas as coverage and raises
  it to `1 / textGamma` (1.45) before it scales the premultiplied colour.
  The value is tuned by eye; see `docs/decisions.md`.

### Glyph atlas allocator

`src/atlasallocator.h` is a private header (the test binary adds `src` to its
include path for it): `AtlasAllocator` tracks which `GlyphKey` occupies which
pixel rectangle of an atlas texture of a given size, with no GPU, FreeType or
`ase` dependency. The caller uploads pixels and grows the atlas by replacing
the allocator.

- **Packing is shelf-based.** Shelves are cut top-down from the unused strip
  at the bottom; a glyph goes on a shelf whose height is its own, rounded up
  to a multiple of 4 px, plus 2 px of padding (so 13 px and 16 px glyphs share
  an 18 px shelf). Each shelf keeps a sorted free list of x-ranges; an insert
  takes the best-fit range on a matching shelf, else cuts a new shelf.
- **Padding is inside the allocation.** Every allocation is 2 px wider than
  the glyph and spans the whole shelf height; the returned `AtlasRect` is the
  1 px inset at the requested size. `GlyphAtlas` uploads the padded
  (w + 2) x (h + 2) block around it with a zero border, so a slot never
  keeps a previous occupant's texels at its edges.
- **Eviction is LRU, bounded by frames.** Entries sit in an index-linked list
  over a vector (no per-insert vector growth once warm); `find`/`insert` move an
  entry to the front and stamp it with the current frame, and an entry stamped
  with the current frame is never evicted. Because touching moves to the front,
  a pinned tail means everything is pinned, which is the only case `insert`
  fails.
- **Only trailing shelves are released.** An evicted range is coalesced back
  into its shelf's free list. A shelf that becomes entirely free is returned to
  the unused strip only while it is the one adjacent to the strip (the while
  loop then takes any free shelves behind it); an entirely free *interior*
  shelf stays a shelf of its height and is reused by glyphs of that height.
  This keeps shelf indices stable and the release logic trivial; the cost is
  that a run of empty interior shelves cannot be re-cut at a new height until
  everything below them has also gone. Relayout on growth, not here.

### Glyph atlas

`src/glyphatlas.h` (private, like the allocator) puts an `AtlasAllocator`
behind a `FORMAT_R8` texture of its `ase::RenderContext`: `get` rasterises a
missing glyph through `Font::rasterizeGlyph`, places it, and queues the
bitmap; `endFrame` pushes the queued `pushSubUpload`s into the command buffer
the caller then pushes the frame's draws into. The atlas starts at 512 px
square and doubles up to 2048.

- **Uploads stay in the draw buffer.** `render()` (`rendering.cpp`) calls
  `endFrame` on the very `CommandBuffer` it then pushes the draws into, ahead
  of them. Several `render()` calls accumulate into one buffer before it is
  submitted, and the GL queue sorts only the runs of draw commands between
  non-draw commands, so an in-line upload is a fence: a later `render()` into
  the same buffer cannot overwrite texels an earlier one's draws still sample.
  Routing the uploads through a separate buffer would lose that ordering.
  (The painter's uniform upload is the opposite case: written once per frame
  into its own ring slot and submitted ahead of the whole draw buffer, it
  needs no place among the draws; see "Painter".)
- **Overflow is deferred to the frame end.** Quads already emitted reference
  the current texels, so a failed insert only flags the atlas: every further
  miss that frame returns nullopt (hits still serve), and `endFrame` then
  either doubles the atlas (new allocator *and* new texture, everything
  forgotten) or, at the maximum size, clears it. The uploads queued before
  the overflow still go out, to the texture the draws used.
- **Slot metadata outlives the allocator's entries.** Bearings live in a
  `GlyphKey -> GlyphSlot` map beside the allocator, which evicts silently, so
  a hit is confirmed with `AtlasAllocator::find` and stale entries are swept
  once the map reaches twice the live count plus 64 entries. Blank glyphs (a
  space) are in
  that map only, never in the allocator or the texture.
- **The atlas keeps its fonts alive.** `GlyphKey::font` is the address
  `Font::getIdentity` returns, and `FontManager` caches `FontImpl` by weak
  pointer, so a dropped font's address could be reused; the atlas holds a
  `Font` per identity it has keyed until it next clears.

For cross-cutting rules (symbol visibility, the include-dir firewall) see
`docs/conventions.md`; do not restate them here.
