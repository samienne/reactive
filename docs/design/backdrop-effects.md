# Design: backdrop effects over an always-offscreen target

> **Status: implemented.** Every window frame renders into one full-screen
> offscreen target and is presented with a quad; a `Drawing` stays a flat,
> ordered list in which backdrop scopes are declared by whoever stacks one
> child over another; an effect such as frosted glass is a small graph of
> shader passes that reads a snapshot of the frame taken when its scope was
> entered and paints its shape back into the frame in paint order.
> Verified against `f541ac65` (2026-11-12). The pass uniform blocks and
> multi-sampler passes build on `shader-interface.md`.

## Why

A frosted-glass card has to show what lies behind it, blurred and tinted.
Three things stand in the way of answering "what lies behind": GL does
not let a draw reliably sample a window's default framebuffer, so a frame
drawn straight onto the window cannot be read back; a flat list whose only
ordering information is element order cannot say which earlier elements
are an element's backdrop; and a container that only concatenates its
children's drawings makes a card in a vbox and a card in a popup over the
same vbox indistinguishable element lists.

Three choices follow from those three obstacles, and the rest of this document
is their consequences:

- **Render every frame offscreen**, into a texture a later draw can sample,
  and present it with one textured quad. One code path for every frame,
  whether or not it uses effects.
- **Keep the drawing flat**, with no z, layer or elevation data on elements,
  and add one nesting element - a backdrop scope - that says "effects in
  here see the frame as it was when this scope was entered". Scopes are
  declared by containers that stack children, never inferred from geometry.
- **Describe an effect as a pass graph** over the scope's snapshot, with
  scratch textures allocated by liveness, rather than analysing which items
  a given effect overlaps.

## Goals

- Frosted glass and tint behind a widget's shape, with animated parameters,
  at a cost that scales with the number of effects on screen rather than
  with the number of elements under them.
- Correct by construction: a glass surface never samples its own content or
  its siblings, nested stacks cannot leak into following siblings, and
  nested flat containers draw the same elements as one flat container.
- Zero cost when unused: a frame with no effects records its content draws
  and the present quad, with no barrier; a scope with no effect in it
  records nothing.
- DPI-independent placement: an effect's shape and regions follow the
  scaling factor. Its pixel parameters (a blur radius, a highlight width)
  are device pixels; converting them from logical units is an open
  question.
- Testable headless: the dummy backend sees every command, so command
  structure, regions, scratch allocation and uniform bytes are asserted in
  CI; only the pixels need a GL driver.

## Non-goals

- User-supplied shader source as public API (the pass model allows it; the
  built-in effects are the only authors today).
- A merging effect group with visual meaning (several glass surfaces sharing
  one backdrop sample, as Apple's `GlassEffectContainer` does); a possible
  later addition, not a performance knob.
- Shape-mask-limited passes, `glCopyImageSubData` for the snapshot copy, an
  `auto` divisor from a tap budget, and per-node caching of a built effect:
  all listed under *Open questions* and *Later*.
- Rendering text at reduced resolution: passes only sample, so text in the
  main target is unchanged.

## The window target and present

Every window owns one full-screen offscreen target of the window's pixel
size: an sRGB colour texture over a depth-stencil renderbuffer, held by the
painter as a `TargetImage`, made on first use and remade when the pixel
size changes (nothing resizes in place; two frames at one size share the
texture). A frame clears and draws into it as it would into a window's
framebuffer, then the painter pushes a barrier and one draw of a static
clip-space quad into the window's default framebuffer with the target
texture bound. The window is never cleared - the quad covers it - and the
present pipeline neither tests nor writes depth. Image frames (rendering
into a caller's target) take the same draw path and carry no quad.

Because the target and the window are both sRGB surfaces under
`GL_FRAMEBUFFER_SRGB`, content blends in linear light in the target, the
present quad's sample decodes, and the window write re-encodes; shaders
never handle gamma. WGL now asks for an sRGB-capable pixel format and a 3.3
context, as GLX already did, so window, target and scratch textures encode
alike. If a platform refused an sRGB window format the present fragment
shader would be the place for a manual encode; none is implemented.

Why always offscreen rather than only when effects are present: a single
path means one set of command shapes to test, no mode switch when the
first glass appears mid-animation, and the extra quad is one full-screen
draw a frame, far below the cost of the frame itself.

## The drawing: a flat list with scopes

A `Drawing` is an ordered list of elements, and order is the only ordering
data. Depth is assigned from paint order, opaque content batches
first, blended content draws back to front, and elements carry no z, layer
or elevation fields. Two things are added.

**Backdrop scopes.** A `BackdropScope` element wraps a sub-drawing with its
own transform, shaped like the clip element without a clip. Entering a
scope conceptually snapshots the frame; an effect directly inside the scope
(not inside a nested scope) samples that snapshot; on exit the enclosing
scope's snapshot is current again; the root of a drawing is an implicit
scope. A glass element therefore sees everything painted up to its
enclosing scope's entry and nothing after it - never its own content, never
its siblings, never a nested stack's content. Scopes nest, concatenation
keeps them as single elements so nesting survives composition, and
clipping and transforming a drawing apply to the scopes inside it. A scope
caches its contents' bounds so bounds queries do not recurse.

**Pending shadows.** A surface's shadow belongs under the content of the
container that holds the surface, not merely under the surface itself: a
card's shadow should fall on the vbox around it, behind the neighbouring
cards. A drawing therefore carries a second element list, the pending
shadows. Concatenation appends both lists, clip and transform apply to
both, the bounds cover both, and `flush` moves the pending shadows ahead of
the main list. The render tree root flushes the drawing it returns; anything
that renders a drawing directly must flush first, and the renderer asserts
that nothing is pending. Shadows are ordinary blended elements; this list
is the whole mechanism - no levels, no layer ids.

**Painting over a child.** One helper, `Drawing::over`, is the single
composition for putting one drawing on top of another: the lower drawing's
main list, then the upper drawing's pending shadows, then the upper
drawing's main list wrapped in a scope; the lower drawing's shadows stay
pending. Shadows go *before* the scope on purpose, so a glass sibling
samples the shadow rings in its backdrop (placing them after the scope was
considered and not chosen). The rule for callers is in `conventions.md`:
**whoever paints one child on top of another is a stack**, and every such
place goes through this helper unconditionally, because an unused scope
costs nothing and the author of a container cannot know what its children
will hold. Same-level overlap without a stack (negative margins, say)
samples the enclosing snapshot; that is documented, not analysed.

**The effect element.** An `EffectElement` is a shape, a shared pointer to
an immutable effect and a transform. The shape stays in the element's own
space and the drawing's transforms compose into the element's transform, so
the renderer still sees the element's own scale, which converting the
effect's parameters from logical units would need (see *Open questions*).
The effect is shared, never copied, so
one effect keeps its identity across elements and frames. Bounds are the
transformed shape bounds with no padding; the margin the passes sample is
derived at render time. A clip wraps the element like any other: the
renderer intersects its bounds with the clip rectangle before deriving
regions.

## The render tree: surfaces and stacking containers

A `SurfaceNode` is a filled shape under a child subtree with an optional
shadow and an optional backdrop effect, all animated as values (the shape
function, the fill brush, the shadow's offset, spread and brush, the
effect's options) and never as structure, so a hover that lifts a card
animates its shadow size without anything popping. Drawing it adds the
shadow - the shape moved by the offset, filled with the shadow brush and,
for a positive spread, stroked with a pen twice the spread wide - as a
pending shadow, then the effect element, the fill and the child's drawing
to the main list in that order. It never adds a scope: that would give
every card in a vbox a private snapshot and a vbox of cards would see no
backdrop at all.

A `ContainerNode` takes a stacking flag. A flat container (vbox, hbox,
grid) concatenates its children's drawings and passes their pending shadows
up unchanged, so nested flat containers draw the same elements as one. A
stacked container paints its first child flat and each later child through
`over`, so each later child's backdrop effects sample the children below it
and their shadows but nothing of itself, and a nested stack's scopes stay
inside its own scope. The flag is the only difference between the two;
there is no separate stack node class.

In `bqui`, `widget::stack` and `widget::layout` with the stacked flag build
a stacking container; the drawing modifiers that paint behind or over their
widget (`onDraw`, `onDrawBehind`, `background`, `foreground`, and `frame`
through `background`) are stacking containers too, and the window root is
its content over a background, so every window's content already sits in
one scope over the window colour. `modifier::surface` wraps the widget's
render tree in a surface node; `frostedGlass`, `tint` and `backdrop` are
shorthands for an effect alone. A surface adds no scope of its own: it is
layered by the stack or background it sits in.

## Effects as pass graphs

An `Effect` is a list of passes in execution order, hence a DAG by
construction. Each pass is a fragment shader with its declared interface,
a list of inputs that name either the backdrop (the enclosing scope's
snapshot) or an earlier pass's output, an optional output (a scratch
target at an integer divisor of the screen size - 1, 2, 4 or 8 - in one of
the sRGB, R8 or RGBA16F formats), a geometry, a padding in pixels of its
own output for how far outside its region it samples, and an optional
parameter block. Two geometries exist: a *region* pass draws a quad over
its computed region into its scratch output; the *shape* pass, always the
last and only the last, draws the effect's triangulated shape into the
main target and has no output. Construction validates and reports every
broken rule at once: a missing or misplaced shape pass, an input that is
not the backdrop or an earlier pass, a region pass without a readable
output, an output nobody reads, a negative padding, a parameter block that
does not match the shader's declaration. A plain tint is a single shape
pass reading the backdrop.

**Regions are derived backwards.** Planning starts from the shape's device
bounds (its transformed bounds through the enclosing transforms and the
scaling factor, intersected with any clip rectangle, snapped outward to
whole pixels and clamped to the target) and walks to the first pass: a pass
writes the union of what its readers sample, each reader's region grown by
that reader's padding and rescaled to the writer's divisor with the origin
floored and the far edge ceiled, clamped to the pass's own target size. The
union of the backdrop reads, at full size, is the region the scope's
snapshot copy must cover. This gives the exact texels each pass writes and
reads, so a pass never samples a stale texel and the snapshot never copies
more than it needs; screen edges clamp.

**Scratch is allocated by liveness over one timeline.** A `ScratchTimeline`
lists a frame's scope snapshots and effects in execution order, each effect
naming the snapshot it reads. Planning numbers every snapshot and pass
output as a step, finds each value's last reader, and runs a linear scan
per (divisor, format) pool: a pass takes its output texture *before* its
inputs are released, so an output never aliases an input of the same pass
and the GL feedback-loop rule holds without a barrier; the lowest free
texture is reused first. A chain ping-pongs between two textures, a wide
graph takes one per live value, consecutive effects reuse the same
textures, and a snapshot stays live until its last reading pass. Scope
snapshots are divisor-1 entries in the same pools, so the number of live
snapshots is the scope stack depth, two being the common worst case.

**Scratch textures are pooled per window.** The `ScratchPool` holds a pool
per key, every texture full-screen at its key's divisor and each in a
framebuffer of its own with no depth attachment. A slot's texture is made
on first acquisition, so a frame allocates exactly what its plan asks for;
textures persist across frames, a new screen size drops them all, and a
texture no frame has acquired for 120 frames is released, so a window that
stops showing effects gives the memory back. Nothing is ever cleared: a
region quad overwrites its whole region, later passes read only inside it.
Memory at 1920x1080 with two full-size snapshots and two divisor-4 scratch
textures is on the order of 40 MB per window; at 4K about 145 MB.

**Pipelines are cached per (fragment source, target format, geometry)** in
a `PassPipelineCache`. Parameters are not in the key, so an animated radius
or tint keeps its pipelines: the shader source is constant across parameter
values by design, and only the parameter bytes change per draw. Every pass
pipeline draws with depth test and write off; a region pipeline does not
blend (the quad overwrites), and the shape pipeline blends source alpha
over the main target; the built-in effects write alpha 1, so their result
is opaque. Each entry hands out a fresh uniform set per draw per
frame and recycles them at frame start, because a set is read when its
draw executes and two draws of one pipeline sharing a set would both see
the ranges bound last.

## The pass shader contract

Every pass shader is written against one contract (`effectshader.h`). The
vertex stage is a single GLSL 330 shader taking a position in the pass's
target pixels; a fragment shader is a generated prelude plus a body. The
prelude and the declared `ShaderInterface` are produced from the same
lists, so the GLSL and the declaration cannot drift: one std140 block per
pass with the target size, target divisor, region and one texel entry per
input (its texel size and divisor) that the renderer fills per draw; an
optional parameter block for the effect's own values; one sampler per
input at the unit of the input's index, so the draw's positional texture
list is the binding, as `ase` requires. At most four inputs. The GLSL
carries no binding qualifiers; `ase` applies the declaration at link.

Fragments locate themselves through `gl_FragCoord` rather than a varying:
the fragment centre times the pass's divisor is a screen position, and
dividing it by an input's size in screen pixels is that input's uv. With
equal divisors a fragment hits its texel's centre; with an input twice as
fine it hits the corner shared by a 2x2 block, so a plain bilinear fetch is
a box downsample and the copy pass needs no padding. Because nothing but
the fragment coordinate and the pass block place a fragment, the same
fragment shaders work under whichever vertex stage the renderer uses for
the shape pass.

Effect parameters travel as bytes in `UniformValues` of the shader's
parameter block, filled when the effect is built from logical units already
converted for the divisor, and uploaded as they are for every draw. This
keeps the shader source independent of parameter values.

## The renderer: pre-pass, timeline, segments

The scope pre-pass is the existing mesh pass. The renderer flattens a
drawing once, in paint order, into items: meshes, text quads, a scope entry
or an effect draw. It opens the implicit root scope first; the scope
visitor appends a scope record, emits its entry item and recurses with that
scope current, while a clip keeps its parent's scope. The effect visitor
maps the element's bounds to device pixels, drops the element when nothing
is left after clamping, and otherwise derives the regions, marks the
current scope used, unions the effect's backdrop region into the scope's,
and fills the shape to a region at the element's point size and
triangulates it into a device-pixel triangle list. Culling applies to
scopes and effects as to any element, so a culled scope has no record and
a culled effect uses none.

After the walk the renderer builds the timeline from the items in order - a
snapshot per *used* scope entry, an effect per draw naming its scope's
snapshot. When it holds no effect, everything is drawn as one batch: no
emitter is made and the commands are those of a drawing without scopes.
Otherwise the scratch plan runs once, the painter hands out an
`EffectEmitter` over the frame's command buffer and the main target, and
the items are walked again: meshes gather into a segment; a used scope
entry and an effect draw each **flush the segment into draws first** and
then emit the snapshot (one quad draw with the copy shader, scissored to
the scope's backdrop union, between two barriers) or the effect's passes.
The emitter's barriers bracket every one of its draws, so the flushed
content is a run of its own before the snapshot and the next segment a run
after the shape pass: the snapshot sees exactly what paint order put below
the scope, and the shape pass lies between the content below and above it.
Z is still assigned over the whole frame, so the depth test orders opaque
content across segments, and a frame with no effects is one
segment with the same z values and no barrier.

Per pass, emission acquires the input textures the allocation names (a
backdrop input resolves to the snapshot's slot), fills the pass block,
writes it and the parameter bytes into the painter's uniform ring, binds
them on a fresh uniform set, uploads the vertices (six for a region quad,
the triangle list for the shape pass), pushes the draw with the textures in
input order and a scissor on the region (the shape draw too, so stray
triangles never leave the derived bounds), and ends with a barrier. A
barrier is skipped when the buffer already ends in one, so a snapshot
followed by its effect reads barrier, copy, barrier, pass, barrier, and so
on. An effect whose shape region is empty records nothing; a pass with an
empty region is skipped. Synchronisation is GPU-internal (a render-target
to texture dependency with an FBO switch between), with no CPU waits; the
barrier is only a command-buffer run split, and the GL loop skips it
without a GL call. Pass and snapshot counters are kept per frame.

## The built-in effects

**Frosted glass** is four passes, all scratch in RGBA16F at one divisor: a
copy of the backdrop (padding 0, the bilinear box downsample above), a
horizontal and a vertical Gaussian (padding equal to the tap radius, sigma
half the radius in texels, floored so a zero radius still divides), and the
shape pass reading only the blurred result (padding of half a blurred
texel, for the bilinear upsample). The divisor is the smallest of 2, 4 and
8 at which the per-pass radius stays within 16 texels, so a blur pass is at
most 33 taps up to a 128-pixel radius; beyond that the tap count grows at
divisor 8. Because the shape pass does not read the backdrop, the snapshot
is released after the copy pass. The look is a saturation scale, a tint by
strength and an edge highlight blended in over a width measured from the
pass's region - an approximation an SDF would replace, kept as the
parameter slot for one. **Tint** is one shape pass reading the backdrop and
mixing a colour in by strength times the colour's alpha, writing alpha 1.

## What each layer owns

- **`ase`** supplies the enablers and nothing effect-specific: a barrier
  command that splits the draw-sort run without GL work; a scissor
  rectangle on draw and clear commands; depth test and write as pipeline
  state rather than a rule derived from blending; the RGBA16F format and a
  sized sRGB internal format; an sRGB-capable WGL pixel format and a 3.3
  context; the declared shader interface and the uniform ring the passes
  bind through; and, per the recorded decision, pipelines that declare the
  formats of the targets they draw into.
- **`avg`** owns the model and the renderer: the scope and effect elements,
  pending shadows and `over`; the pass graph, its validation, region
  derivation and liveness planning as pure functions; the scratch pool,
  pipeline cache and emitter; the window target and present quad in the
  painter; the scope pre-pass and segment flushing in the renderer; the
  surface node, the stacking flag and the built-in effects.
- **`bqui`** owns the modifiers and the rule: `surface` and its shorthands,
  `stack`, and the routing of every modifier that paints behind or over its
  widget through a stacking container. It never builds a scope by hand.

## Alternatives considered

Target and effect model:

- **Region passes with re-rendering** - sort, analyse which items each
  effect overlaps, and redraw them into a region-sized texture per effect.
  Rejected: analysis cost proportional to items times effects per frame,
  items under k effects drawn k+1 times, and text re-laid-out per pass.
- **A second colour target through MRT** - write every draw to two
  textures and sample the second. Rejected: double colour bandwidth on
  every draw for a feature that is rare per frame.
- **Window framebuffer read-back** before each effect. Rejected by the
  owner: GL window surfaces are not reliably readable or consistent, and it
  couples the effect path to the platform surface.
- **Ping-pong full-screen main targets** swapped per effect. Rejected: a
  full-screen copy per effect, where the pass graph copies only the region
  it needs.
- **One pooled uniform set per pass.** Rejected: a set is read when its
  draw executes, so two draws of one pipeline in a frame would both see
  the ranges bound last.
- **A scratch pool that never shrinks.** Rejected: a window that stops
  showing effects would keep its textures for the session.
- **An opaque, depth-writing shape pass** drawn through the mesh path.
  Rejected: it ties the pass to the mesh pipeline's depth rules and rules
  out an effect whose final pass writes a translucent result.

Ordering:

- **An explicit integer z per element with a stable sort.** Rejected: z is
  ordinal, not geometric; it serialises vbox siblings with glass, breaks
  batches, and makes effect edges asymmetric.
- **A geometric dependency scheduler** with layers, deferral and a soft
  padding ring. Rejected: correct but inferred from bounding boxes,
  approximate in the ring, and unnecessary once containers declare
  stacking.
- **Elevation levels inside the drawing** (a vector of levels with shadow,
  surface and content phases). Rejected: duplicates hierarchy the render
  tree already has and needs level counts in stacks.
- **A flat barrier element or a per-element barrier flag.** Rejected: it
  leaks - a nested stack's barrier becomes the latest barrier for the
  following siblings, and the flag is lost on concatenation.
- **Push/pop barrier pairs.** Rejected: unbalanced on concatenation.
- **Named barriers.** Rejected: id uniqueness across independently composed
  drawings.
- **Author-declared parallel sibling branches** in the drawing. Rejected:
  authors cannot know pixel overlap, and the performance cliffs are theirs
  to find.
- **A z for the shape pass** to order it against content, instead of
  segment flushing. Rejected: the shape pipeline neither tests nor writes
  depth, so z would not order it against opaque content anyway, and a run
  boundary costs nothing when no effect is drawn.
- **Scopes added by surfaces** rather than by containers. Rejected: every
  card in a vbox would get a private snapshot and see no backdrop.
- **Shadows after the scope** in `over`. Considered and not chosen: a glass
  sibling should sample the shadow rings in its backdrop.

## Open questions

- **Snapshot vocabulary for stacked containers.** A render tree snapshot
  still reports a stacked container as a plain container; whether tooling
  needs to see the stacking flag is undecided.
- **Effect parameters in logical units.** An `Effect` is built from its
  options before any scaling factor is known, and its divisor, paddings
  and parameter bytes are fixed at construction, so a frosted-glass radius
  or highlight width is in device pixels: the same options blur half as
  wide, relative to the content, on a 2x display. Converting from logical
  units needs the effect built, or its pixel quantities resolved, per
  scale at render time.
- **Per-node effect cache.** The surface node rebuilds its effect each
  frame from the animated options; caching it while the options are
  unchanged is pending a measured need. The shader source does not depend
  on the values, so the pipeline cache is unaffected.
- **GPU-only verifications.** No GL context runs in CI, so these are manual
  WGL/GLX checks (`lw run app`): the orientation of pass regions and
  scissors against the fragment coordinate; whether the blur looks right at
  each divisor; whether an FBO switch between a pass and its reader is
  sufficient synchronisation on every driver, as the GL spec says, or a
  texture barrier is ever needed; and whether the sRGB encode on the WGL
  window matches the target so the present quad is a no-op visually.
- **Memory at 4K and multi-window.** The per-window pool is sized by the
  frame's maximum simultaneous live count; several 4K windows with effects
  may want a shared pool.
- **Effects per frame on tiled GPUs.** Each pass is a pipeline bubble and
  an FBO switch; desktop GL copes with tens, a tiled GPU pays a tile flush
  per switch. The per-frame counters exist for profiling before any
  optimisation.

## Later

User-supplied shader source as public API; an `auto` divisor from a tap
budget; a copy-image snapshot if profiling shows the quad; shape-mask-limited
fills; a merging effect group with visual meaning.
