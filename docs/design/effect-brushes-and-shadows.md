# Design: effect brushes, effect elements and outline shadows

> **Status: design (ahead of the code).** Nothing here is implemented. It
> records decisions taken with the owner on 2026-11-14, including those
> from the owner's review of the first draft, and grounds them in the code
> at the tip of the backdrop-effect stack (PRs #172-#177), which lands as
> it is; this work starts after it. A brush becomes a colour or an effect
> description; a `Drawing` gains an effect element whose inputs are
> sub-drawings or the enclosing scope's backdrop; the renderer lowers both
> into one shared pass graph; shadows are produced by a shadow boundary from
> outline shapes the render tree carries next to the drawing. Builds on
> `backdrop-effects.md` and `shader-interface.md`. Current-state claims are
> verified against `03314fc6` (2026-11-14) unless marked otherwise; the
> section "Work implied by the code" lists what the decisions cost in the
> existing code.

## Why

The backdrop-effect stack made frosted glass work, and three limits of its
shape show as soon as it is used:

- **An effect is a property of one node kind.** Only `SurfaceNode`
  (`src/avg/include/avg/rendertree/surfacenode.h`) can place an effect, and
  it selects one through the closed variant `SurfaceEffect`
  (`SurfaceFrostedGlass` or `SurfaceTint`), mirrored signal for signal in
  `src/bqui/include/bqui/modifier/surface.h`. Glass cannot fill a stroke, a
  text run or any shape that is not a surface, and a new effect means a new
  variant alternative in two libraries.
- **An effect can only read the backdrop.** `PassInput`
  (`src/avg/include/avg/effect.h`) is the backdrop or an earlier pass of the
  same effect. Nothing can blur, fade or mask a widget's own content, so
  group opacity, content blur and a soft shadow have no place in the model.
- **Shadows are painted geometry with no blur and no knowledge of each
  other.** `SurfaceNode::draw` (`src/avg/src/rendertree/surfacenode.cpp`)
  adds the shape translated by the offset as a pending shadow: one
  `ShapeElement` filled with the shadow brush and, for a positive spread,
  stroked with a pen of twice the spread in the same brush. Two defects
  follow directly: overlapping shadows of two surfaces blend twice and
  darken, and with a translucent brush the fill and the stroke of one
  shadow overlap along the edge and blend twice there as well.

A fourth limit is cost. Scratch targets are full-screen at a divisor
(`src/avg/src/scratchpool.cpp`), a blur chain belongs to one effect element
and is never shared, a snapshot is shared only inside one backdrop scope,
and every pass reruns every frame. The owner measured the stack tip at
800x600 with zero, one, two side-by-side and two overlapping glass panels
(numbers from the design discussion, not re-measured for this document):

| | 0 | 1 | 2 side by side | 2 overlapping |
|---|---|---|---|---|
| Passes | 2 | 7 | 11 | 12 |
| Snapshots | 0 | 1 | 1 | 2 |
| Scratch targets | 0 | 3 | 5 | 5 |

## Goals

- Glass, and any later effect, usable wherever a brush is accepted: fills,
  strokes and text.
- One effect model for backdrop effects and for effects over a widget's own
  content, lowered into one pass graph.
- Identical work done once: two panels with the same blur over the same
  backdrop share one blur chain.
- Soft shadows that do not darken where they overlap and do not blend
  twice along their own edge.
- Cost that follows the shapes on screen, reached in measured steps, with
  the measurements in place before the first optimisation.

## Non-goals

- A stable, user-facing way to author effects. Effects stay built by
  factory functions inside `avg`; public authoring comes later.
- Damage tracking and reuse of results across frames. It is the last
  staged cost option and a separate project.
- Per-caster shadow parameters (elevation) and translucent casters. Both
  are recorded as later routes, not designed here.
- Changing PRs #172-#177. They land as they are.

## Where the code is today

Verified by reading the files named; `src/avg/AGENTS.md` (*Shapes and
drawings*, *Render tree*, *Effects*, *Text*) describes the same state in
more detail.

- **Effect model.** `Effect` (`src/avg/include/avg/effect.h`) is a public,
  validated list of `Pass` values in execution order. A pass names its
  inputs (`PassInput`: backdrop or an earlier pass, at most
  `maxPassInputs`), an optional `PassOutput` (a `Divisor` of the screen size
  and a format), a `Geometry` (`Region` quad or the final `Shape`), a
  padding and its parameter bytes. Only the last pass has `Shape` geometry
  and it draws into the main target.
- **Built-ins.** `src/avg/src/builtineffects.cpp` builds two effects:
  `frostedGlass` (a backdrop copy, a horizontal and a vertical blur, a
  Shape pass with tint, saturation and highlight) and `tint` (one Shape
  pass reading the backdrop). There is no enum: the kind is the alternative
  held by the `SurfaceEffect` variant, turned into an `Effect` each frame
  by `makeEffect` in `surfacenode.cpp`.
- **Drawing.** `Drawing` (`src/avg/include/avg/drawing.h`) already has an
  `EffectElement` (shape, shared `Effect`, transform) and a `BackdropScope`.
  It carries a second list, the pending shadows; `Drawing::over` and
  `flush()` (`src/avg/src/drawing.cpp`) place them under content.
- **Rendering.** `generateMeshesForElements` in `src/avg/src/rendering.cpp`
  flattens the drawing into meshes, text quads, `ScopeEntry` and
  `EffectDraw` items, deriving every effect's regions (`deriveRegions`,
  `src/avg/src/effectplan.cpp`) and marking its scope used. `render` builds
  a `ScratchTimeline`, `planScratch` allocates scratch by liveness, and
  `drawItems` flushes the gathered meshes before each used scope entry
  (`EffectEmitter::emitSnapshot`) and each effect (`emitEffect`,
  `src/avg/src/effectemitter.cpp`).
- **Brush.** `Brush` (`src/avg/include/avg/brush.h`) is a colour and nothing
  else, with equality, ordering, hashing and `lerp`
  (`src/avg/src/lerp.cpp`). `Pen` holds a `Brush`.
- **Text.** `generateTextQuads` in `rendering.cpp` holds the rules for
  drawing a `TextEntry` from the glyph atlas; an entry that fails one goes
  through `Font::textToPath` and is tessellated like a shape.
- **Shape union.** `Shape::add` (`src/avg/include/avg/shape.h`,
  `src/avg/src/shape.cpp`) is a lazy boolean union evaluated when the shape
  is filled to a `Region`; `Region::operator|`
  (`src/avg/include/avg/region.h`) is the evaluated form. Both end in
  Clipper's `ctUnion` (`operatePaths` in `src/avg/src/region.cpp`).
  `Region::offset` grows a region, which is what a spread needs.

## Decisions

### 1. A brush is a colour or an effect description

`Brush` becomes a value holding either a colour or an effect. Filling or
stroking a shape with an effect brush does what an `EffectElement` with
that shape does today: the effect's final pass paints the fill or the
stroke geometry. Glass can therefore be used anywhere a brush is accepted,
including a `Pen` and a `TextEntry`.

"Behind" keeps the meaning the backdrop stack gave it: an effect brush
reads the backdrop of the nearest backdrop boundary (`BackdropScope`), not
the earlier siblings inside that boundary.

An effect brush does not batch with colour draws; it is an effect draw
with its own run, as an `EffectElement` is today.

When a brush animates between two effects of the same kind, the parameters
interpolate, as `SurfaceEffect` does member by member today. Any other
pair (different effect kinds, or a colour and an effect) is a sharp
transition with no crossfade: the brush switches to the new one at the
start of the transition (the midpoint is the alternative). The result of
an interpolation is therefore always a single `Brush`, which is what
`Animated<Brush>` produces through `lerp`; no brush form holding two
brushes and a weight is needed.

### 2. Effects are opaque values built by internal factories

An effect is an opaque value. The only way to make one is a factory
function in `avg` (free functions, as `frostedGlass` and `tint` already
are). A factory returns a pure description and needs no GPU context;
pipelines are compiled and cached lazily by the renderer, as
`PassPipelineCache` does now.

Each factory assigns its passes a shader identity, a cheap value that
stands for the shader source. Two passes are the same when their shader
identities are equal and their parameter blocks, inputs and outputs
compare equal byte for byte. This is what chain sharing (decision 4)
compares, so the renderer never compares GLSL text.

The closed selection goes away: the `SurfaceEffect` variants in `avg` and
`bqui` are replaced by passing the effect value (inside a brush, or to an
effect element). `Pass`, `PassInput` and `PassOutput` stop being the public
way to describe an effect. A stable user-facing authoring model is out of
scope.

### 3. An effect element in the drawing

The drawing gets an element that generalises today's `EffectElement`. It
holds:

- a list of inputs, each a sub-drawing or the symbolic backdrop of the
  enclosing scope;
- one pass with its parameters;
- an optional shape;
- an output scale and format.

A multi-pass effect is nested elements: the inner element is the
sub-drawing input of the outer one. A list of inputs is allowed from the
start, because liquid glass needs the backdrop together with a blurred
mask of its shape, whose gradient gives the edge direction and the bevel
height.

Uses beyond backdrop effects: group opacity (a sub-drawing composited at
an alpha), content blur, and the blur of a shadow.

An effect brush and an effect element lower into the same pass graph. The
brush is the short form for "one backdrop effect painted through this
geometry".

**Rendering a sub-drawing.** A sub-drawing is rendered into a scratch
target by the ordinary mesh path, which draws opaque elements first and
relies on the depth test, so the target needs depth:

- A scratch target gets a depth attachment only while it is the target of
  a sub-drawing. Quad (`Region`) passes need none.
- One pooled depth buffer per target size is reused by every sub-drawing
  of that size. Depth is needed only while the sub-drawing is drawn and
  is never read afterwards, so nothing has to outlive the draw.
- The ordering rule that makes the reuse safe: a sub-drawing is
  self-contained, and the sub-drawing inputs nested inside it are rendered
  before the outer sub-drawing starts. No sub-drawing is ever interrupted
  by another one that needs the same depth buffer. Backdrop inputs do not
  disturb this, because the scope's snapshot already exists by then.
- On entry each sub-drawing clears colour to transparent and clears
  depth.

**Alpha.** Sub-drawing results are premultiplied, with no extra step. The
painter already premultiplies brush colours
(`src/avg/src/rendering.cpp:171`, `:187`, `:219`) and blends One /
OneMinusSrcAlpha (`src/avg/src/painter.cpp:155-156`), so content rendered
into a layer cleared to transparent is correctly premultiplied. Two rules
follow: the pass that composites a sub-drawing uses the same blend, and
effect shaders treat every input as premultiplied (a backdrop sample is
opaque, so it satisfies this already).

### 4. The drawing stays a tree; the graph lives in the renderer

A `Drawing` stays a tree of elements. The renderer lowers it into a
directed acyclic graph of passes:

- each backdrop input is bound to the content of its scope;
- passes that are the same by decision 2 (shader identity, and byte-wise
  equal parameter block, inputs and output) in that frame are merged, so
  two panels with the same blur over the same backdrop share one chain.

Two reasons put the graph in the renderer and not in the drawing. Widgets
cannot see each other, so no widget can build the sharing. And the
backdrop is expressed in the scope's coordinates: it is a free variable
that the scope binds, so a widget's drawing cannot hold it as a value.

Sharing is kept simple. Matching chains always share, and each shared
pass covers one region: the bounding box of the panels that use it. There
is no logic that splits a chain or decides against merging. Two small
panels far apart therefore blur the box that spans them; drawing one pass
over a set of rectangles is the recorded refinement (see "Later") if
measurements show that case matters.

Two limits are inherent, not defects:

- Overlapping panels cannot share. Each later child of a stacked
  container gets its own scope (`Drawing::over`,
  `src/avg/src/drawing.cpp:496-506`), and the second panel's backdrop
  contains the first, so their inputs differ by definition.
- Sharing needs equal parameters in that frame. The blur divisor follows
  the radius (`frostedGlassDivisor`), so two panels mid-animation with
  different radii share nothing until they settle.

### 5. Regions follow from the shape

Each pass declares how far outside its output region it reads (today's
`Pass::padding`). The renderer starts from the shape's bounds and
propagates inward, growing by each pass's reach, so a backdrop or a
sub-drawing is needed only in the final grown region. An element without
a shape uses its input's bounds grown by its own reach. `deriveRegions`
already does this for one effect over the backdrop; the rule extends to
sub-drawing inputs and to shared nodes, whose region is the bounding box
of what all their readers need (decision 4).

This replaces "everything full-screen" as the route to lower cost. The
options are staged, in this order, each adopted only when the counters
(decision 10) show it pays:

1. **Share blur chains** (decision 4).
2. **Region- or panel-sized scratch targets** in place of full-screen ones.
3. **Reuse results** while the backdrop under a panel is unchanged. This
   needs damage tracking and is a separate project.

Removing the snapshot copy is deferred to "Later"; the reasons are
recorded there.

### 6. Outlines travel next to the drawing

The render tree carries, next to each drawing, the outline shapes of the
widgets that produced it.

- **Set in `bqui`, where a shape becomes a widget.** The code that turns a
  shape into a widget (`src/bqui/include/bqui/shape/shape.h`) also applies
  the outline modifier. `avg` gets a small outline node that passes its
  child's drawing through unchanged and adds the outline. `onDraw` is left
  alone, so a user `onDraw` callback sets no outline unless the user adds
  the modifier.
- **Which shapes set one.** The automatic outline comes from opaque fills
  only. A stroke-only shape's outline is the stroke geometry, not the
  enclosed area. A shape with both a fill and a stroke has an outline
  that includes the stroke width, so the outline matches what is
  painted. A translucent fill sets none, so a hover highlight at
  30% alpha casts nothing, unless the user sets an outline by hand.
- A modifier can override the outline, add to it, or set it to none.
- Text sets no outline by default.
- Casting is opt-out: everything with an outline casts once the outline
  reaches a shadow boundary.
- **An outline that reaches no shadow boundary is dropped.** The root is
  not an implicit boundary; no boundary means no shadow.
- A caster supplies no parameters for now; there is no elevation.
- Outlines receive the same transforms as the drawing.
- **Clips also clip outlines.** An item scrolled out of view must not
  cast, so the caster is cut, not only its painted shadow.

`Drawing` loses the pending-shadow list, and `Drawing::over` no longer
interleaves shadows: it is the backdrop composition only.

### 7. The shadow boundary

A shadow boundary modifier takes the light parameters: direction or
offset, softness and colour. It collects the outlines that reach it,
offsets them, unions them geometrically (`Shape::add`), and emits the
union into the drawing as one fill wrapped in blur effect elements
(decision 3), under the boundary's content.

- Because the shadow is one unioned shape, overlapping shadows do not
  darken twice, and there is no separate stroke to blend against the fill.
- Casters inside one boundary do not shadow each other. That needs a
  nested boundary.
- Casters are treated as opaque.
- The per-surface shadow of the surface modifier goes away
  (`src/bqui/include/bqui/modifier/surface.h:77-82`). The demo
  (`src/testapp1/main.cpp:106`) gets a shadow boundary in its place.

A backdrop boundary is usually wanted at the same point. They are
separate modifiers, plus one combined modifier for the common case, and
the nesting order decides what glass sees:

- **Shadow boundary outside the backdrop boundary.** The shadow stamp is
  painted before the scope is entered, so it is part of the backdrop, and
  glass shows the blurred shadows of its neighbours. The combined modifier
  nests this way.
- **Backdrop boundary outside the shadow boundary.** The stamp is painted
  inside the scope, after its snapshot, so glass does not see the shadows;
  they only lie over the backdrop.

With the separate modifiers the nesting is the user's choice.

### 8. Glass surfaces set their outline to none

A frosted or glass surface sets its shadow outline to none. The effect's
shape and the shadow outline are separate things, so no "subtract the
casters from the shadow" step is needed now.

Subtraction is recorded as the route to translucent casters later: clip
the shadow stamp to the outside of the casters, applied after the blur.

### 9. Text with a non-colour brush uses the outline path

`generateTextQuads` gets one more fallback rule: a `TextEntry` whose brush
is not a colour is not drawn from the atlas. It goes through `textToPath`
and becomes the shape of an effect draw.

The costs are those of the outline path: no antialiasing, and
tessellation every frame. The later option is to combine the brush's
shader with the atlas coverage.

### 10. Instrumentation first, in two parts

Instrumentation is the first step and guides the rest. It is split by
what it costs to build.

First, before any other stage:

- **Per-frame counters**: passes, draws, snapshots, scratch targets,
  scratch bytes and pixels shaded. They are exposed after a frame and
  asserted in the headless test for fixed scenes. Pixels shaded is the
  figure that shows the cost of a wide shared region (decision 4), which
  the pass count hides.
- **CPU time**: Tracy zones for tree update, drawing generation,
  tessellation, shadow union, sort and submit.
- **A fixed scene set**: zero, one, two and N panels; one large panel; 4K;
  idle against one animating widget.

Later, only when a choice between optimisations needs it:

- **GPU time per pass**: timer queries. Draws execute from a command
  buffer after recording, so this is a new `ase` interface, not a GL call
  at record time, and it has to fit the Vulkan-ready design.

### 11. Sizes are in logical units

Softness, and every other size an effect or a light takes, is given in
logical units. A shadow's offset is geometry and scales with the display;
a softness in device pixels would be half as wide relative to it on a 2x
display.

The conversion to pixels happens where the scaling factor is first known:

- **A shadow boundary's sizes are converted at the boundary**, which
  knows the factor when it emits its blur.
- **An effect brush's sizes are converted by the renderer when it lowers
  the brush.** The brush is built before the factor is known; lowering is
  the first place that has both the brush and the factor.

This resolves the open call in `backdrop-effects.md` ("Effect parameters
in logical units"): an effect's divisor, paddings and parameter bytes are
in device pixels today, fixed when the effect is built, before the
scaling factor is known. They move to logical units and are converted at
lowering.

### 12. Sequencing

This is not part of the current stack. The order after it lands is:
counters and CPU zones; effect brush, effect element and lowering with
chain sharing; outlines and the shadow boundary.

## Staged plan

Each stage lands on its own and leaves `master` working.

1. **Counters and CPU zones.** Collect passes, draws, snapshots, scratch
   targets, scratch bytes and pixels shaded per frame in `avg`, readable
   after `render`.
   Assert them in the headless test for the fixed scene set. Add Tracy CPU
   zones where they are missing. No behaviour change.
2. **Opaque effects.** Effects become opaque values from factories, each
   pass carrying its shader identity; `SurfaceEffect` in `avg` and `bqui`
   is replaced by the effect value; same-kind interpolation moves onto the
   value. No rendering change; counters unchanged.
3. **Effect brush.** `Brush` holds a colour or an effect; fill, stroke and
   the text rule (decision 9) lower to effect draws; `lerp` interpolates
   same-kind effects and switches sharply otherwise. `SurfaceNode`'s
   effect member becomes a brush. Effect sizes are given in logical units
   and converted to pixels at that lowering (decision 11). Counters
   unchanged for the existing scenes at a scaling factor of one.
4. **Lowering and chain sharing.** Replace the per-effect timeline with
   the lowered pass graph; merge equal passes inside a scope, each shared
   pass over the bounding box of its users. Counters drop for the
   side-by-side scenes; their expected values change in the same PR.
5. **Effect element with sub-drawing inputs.** Offscreen content
   rendering with the pooled depth buffer and the entry clears of
   decision 3, nested elements, multiple inputs; group opacity and content
   blur as the first users.
6. **Outlines.** The outline node in `avg`; the render tree carries
   outlines, transformed and clipped with the drawing; `bqui` shapes set
   them by the rules of decision 6; the override, add and none modifiers.
   Nothing consumes them yet, so pending shadows still draw.
7. **Shadow boundary.** The boundary modifier with light sizes in logical
   units, the combined modifier (shadow boundary outside backdrop
   boundary), glass outlines set to none, a boundary in the demo. The
   pending-shadow list, `SurfaceShadow`, the surface modifier's shadow
   argument and the shadow half of `Drawing::over` are removed in the same
   PR, since the two mechanisms cannot both own shadow placement.
8. **Cost options** from decision 5, one PR each, each justified by the
   counters: region-sized scratch first. GPU timer queries (decision 10)
   are added at this point if the counters cannot settle a choice.

## Work implied by the code

None of these is open; each is work a decision implies in code that
exists. Line numbers are at `03314fc6`.

- **Brush consumers assume a colour.** `Painter::getPipeline` picks the
  pipeline from the brush's alpha (`src/avg/src/painter.cpp:288-302`);
  mesh and text generation read `getColor()`
  (`src/avg/src/rendering.cpp:187`, `:219`, `:364`); `lerp(Brush, Brush)`
  interpolates the colour (`src/avg/src/lerp.cpp:51-53`); `Brush` has
  `operator<`, hashing and a no-op `operator*(float)`
  (`src/avg/include/avg/brush.h:26-48`). Each gets a meaning for an effect
  brush in stage 3.
- **Passes have no identity or equality.** `makeEffect`
  (`src/avg/src/rendertree/surfacenode.cpp:117`) builds a fresh `Effect`
  per surface per frame, and `Pass`
  (`src/avg/include/avg/effect.h:122-130`) holds the fragment source as a
  string and `ase::UniformValues`, neither comparable. Decision 2's
  shader identity and byte-wise parameter block replace both.
- **The planner is per effect, in sequence.** `ScratchTimeline`
  (`src/avg/include/avg/effectplan.h:195-235`) is a list of snapshots and
  whole effects, and `EffectRegions` is per effect. Liveness allocation
  carries over, but the timeline, `planScratch`, `deriveRegions` and
  `emitEffect`'s per-effect loop are rewritten around graph nodes, and
  their tests (`src/avg/test/effectemittertest.cpp`,
  `src/avg/test/effectrenderingtest.cpp`) pin today's exact command
  sequences.
- **Scratch targets have no depth and are never cleared.** Scratch
  framebuffers have a colour attachment only
  (`src/avg/src/scratchpool.cpp:43-44`), z is assigned over the whole
  frame (`rendering.cpp:763-776`, `:846-849`), and no effect pass clears.
  Decision 3 adds the pooled depth buffer, the entry clears and the
  render order; this is the largest piece of new renderer work and the
  reason sub-drawing inputs are their own stage.
- **`RenderTreeNode::draw` changes for every node.** It returns
  `std::pair<Drawing, bool>`
  (`src/avg/include/avg/rendertree/rendertreenode.h:49-52`), taken by
  every node kind, `RenderTree::draw`, the snapshot path and the tests.
  The outline list needs the operations the pending-shadow list has today
  (`+=`, `clip`, `transform`, `filterByRect`, bounds), so the mechanism
  moves more than it disappears: it holds geometry instead of painted
  elements and has one consumer.
- **Counters are per render.** Stage 1 made `render` return a
  `RenderCounters` (`src/avg/include/avg/rendercounters.h`) collected from
  the frame's emitter and command buffer, which `paintToImage` and
  `paintToWindow` pass on; `src/avg/test/rendercounterstest.cpp` pins
  them for the fixed scene set. The headless test
  (`src/bqui/test/headlessrendertest.cpp`) renders an empty widget and
  asserts frame counts only.
- **Opaque effects against the tests.** `Effect`'s constructor from passes
  is public and exported (`effect.h:161-181`) and the `avg` tests build
  effects from hand-written passes. Making effects opaque needs a
  test-only or internal construction route.
- **Naming.** `Drawing::EffectElement` already exists
  (`src/avg/include/avg/drawing.h:78-83`) with a different shape from the
  element of decision 3. The new element replaces it; it is not a second
  kind.
- **Text and strokes as an effect shape.** The effect visitor triangulates
  `effect.shape` by filling it (`rendering.cpp:634-637`); strokes and text
  outlines need the same route with `strokeToRegion` and `textToPath`
  (`rendering.cpp:527-541`). The effect's final pass does not test depth,
  so glass text is ordered by segment flushing only: every glass text run
  is a run boundary for the content around it.

A separate fix, not part of this design: the pen test at
`src/avg/src/painter.cpp:298`, `std::abs(alpha) - 1.0f > 0.0001f`, is
never true, so pens always take the blended pipeline; the intended test
is `std::abs(alpha - 1.0f) < 0.0001f`.

## Risks

- **The union runs per frame through Clipper.** `Shape::add` is evaluated
  at fill time at the frame's point size (`src/avg/src/shape.cpp:229-230`,
  `src/avg/src/region.cpp:231-255`). A boundary with many casters (a list)
  unions all of them every frame while anything animates. This is why
  "shadow union" has its own CPU zone; no cache is designed.
- **A wide shared region can cost more than the sharing saves.** Pass
  count drops when two far-apart panels share a chain, while the pixels
  blurred grow to the box that spans them (decision 4). The pixels-shaded
  counter of decision 10 measures this from the first stage, and it is
  the figure for judging the rectangle-set refinement.

## Open questions

- **Per-caster parameters** such as elevation.
- **A stable user-facing way to author effects.**

## Later

- A stable user-facing way to author effects.
- Subtraction of casters from the blurred stamp, for translucent casters.
- Brush shaders combined with atlas coverage for text.
- Reuse of pass results under an unchanged backdrop, on top of damage
  tracking.
- Per-caster elevation.
- Drawing one shared pass over a set of rectangles in place of their
  bounding box, if measurements show far-apart panels matter.
- GPU timer queries (decision 10).
- **Removing the snapshot copy.** The snapshot is a full-resolution copy
  of the main target taken at the scope's entry
  (`src/avg/src/rendering.cpp:863-872`,
  `src/avg/src/effectemitter.cpp:105-124`), and `frostedGlass` then copies
  it again, downsampled (`src/avg/src/builtineffects.cpp:216-217`), so
  frosted glass copies the backdrop twice today; look at this when
  measuring. Removing the first copy needs every backdrop-reading Region
  pass hoisted to the scope's entry, where the main target holds exactly
  the backdrop; run where it runs today, such a pass would see the
  scope's earlier siblings and break decision 1. It cannot apply to
  `tint` (`builtineffects.cpp:238`), or to any effect whose final pass
  reads the backdrop while drawing into the main target: that is a
  feedback loop, so such a scope keeps its snapshot.
