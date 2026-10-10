# Layout: the pure-solver model

*Last verified against `d29aa67f` (2026-10-10).*

How `bqui` sizes and places widgets. The code lives in
`src/bqui/src/widget/constraintbox.*` (containers, fillers, the region solve),
`constraintlayout.*` (bands, flattening, the solver wrapper, strengths),
`src/bqui/src/modifier/constraintsize.cpp` and `pureconstraint.cpp` (size words),
and `src/bqui/src/windowbridge.cpp` (the window root). The descriptor types are
in `include/bqui/widget/layoutspec.h`. The solver is Cassowary, from the vendored
`arrange` subproject.

## The model in one paragraph

Every builder carries a `PureLayout`: per axis, a **band** (named `min`, `max`,
`natural` and `flex` fields) plus a list of untagged solver relations. Size words
replace a band field, so the last writer wins and nothing competes by strength.
A container **stamps** each child's band into plain solver constraints on the
child's box, adds its own structural relations, and publishes an aggregate band
of its own, which the next size word can override again. A **region** reads the
band off its top builder, anchors the outermost box to its assigned size, and
solves the whole subtree once. The solution is handed down into the build, and
each container reads its children's boxes out of it.

## The descriptor

`PureLayout` has three phase functions, each returning `AnySignal<Constraints>`:
`getWidth`, `getHeightForWidth(widthSolution)` and
`getWidthForHeight(heightSolution)`. Phases 2 and 3 receive the whole solution
of the other axis, not a scalar, because a container cannot turn its own width
into its children's widths without solving. It forwards the same solution to
every child, and each leaf reads its own box from it. So a leaf's height can
reflow with its resolved width at any depth.

`Constraints` holds the band fields as *values* (`natural` paired with the
strength it is held at), not as baked constraints. A value becomes a constraint
only when it is flattened (`flattenConstraints`) against the box it ends up on.
That is what lets a wrapper grow a band by arithmetic and re-tag it onto a new
box. `relations` is additive: tiling, inset relations, flex couplings, and the
read-back variables the solver API needs (it cannot enumerate variables).

A builder is minted with an empty `PureLayout`. Leaves publish their own:
`label` publishes its measured text extents as a natural, `textEdit` and
`scrollView` a default size, and `makeBuilderFromElement` its element size.
`simplePureLayout` builds the common shape (a width band plus a height-for-width
function; phase 3 returns the phase-1 width).

## Regions

**One region is one solve.** `buildPureRegion` reads the top builder's
`PureLayout`, flattens its relations (not its band, which would only lose to the
anchor) onto the outermost box, anchors that box to the assigned size, solves
width, then solves height given the width solution, and combines the two. It builds the content with the combined solution as an
explicit build argument. The solution is not a `BuildParams` value, because
params are captured at widget-to-builder time, before the solve exists.

- **Every window root is a region.** `WindowBridge` wraps its content in
  `buildPureRegion`, which also returns the content's band (`RegionBand`: the
  phase-1 width band and the phase-2 height band at the current width).
- **A window opens at its root's natural size.** The root is built and solved
  before the OS window exists: the width is the width band's natural, then the
  height is the height band's natural read at that width. An axis with no
  natural falls back to 800 (width) or 600 (height). `Window::initialSize`
  replaces the natural. Either way the band's min and max win.
- **After opening, the window does not follow its content.** A later change of
  natural leaves the size alone. Only the band's min and max limit: the root is
  laid out at the window's size clamped to them (the height limits read at the
  clamped width), at the window's top-left corner, so it overflows a window
  below its min and leaves a margin in one above its max. The clamp settles in
  at most two re-solves, because height never feeds back into width.
- **A container outside a region throws.** If a container's build gets a
  solution that does not contain its own box, it throws `std::logic_error` on
  first evaluate instead of laying its children out at 0x0. Tests build through
  `buildInRegion()` (`src/bqui/test/purelayouttestutil.h`), which wraps the
  widget in the same region core.
- **Solves are change-gated and from scratch.** `solveLayout` re-solves when the
  constraint signal changes, starting from a cleared solver each time. Only
  structure is `required` (see Strengths), so a solve is always feasible and a
  conflict between stated values resolves by strength. An infeasible spec
  (`arrange::Error`) would mean a bug in a fragment: it is logged and the
  previous solution kept, so on a first solve the region collapses to zero.
- **Conflicts are logged.** After each solve the strong constraints are checked
  against the solution, and any left unmet are logged to stderr as one line
  naming their kind (`region`, `min`, `max`, `fixed`, `align`, `guide`) and the solved value
  against the stated one, only when that set changes.

## Sizing

Size words (`modifier/constraintsize.h`) each set one band field on one or both
axes:

- **`fixedWidth`/`fixedHeight`/`fixedSize`** set a strong `natural` and **clear
  `flex`** on that axis, including a flex that aggregated up from a child. A
  fixed size and a flex are one choice per axis, and the last writer wins: a
  later `fill()`/`grow()` re-enables flex, and the fixed value then rides as the
  flex basis.
- **`minWidth`/`maxWidth`/...** set strong bounds and **leave `flex` alone**.
  They bound a flexing widget's stretch rather than cancel it, and they beat a
  fixed size they contradict: `fixedWidth(300) | maxWidth(200)` is 200 in either
  order, and a `min` beats a `max`.
- **`defaultSize`** sets a natural at content strength, for a leaf with no
  measured size of its own.
- **`fill()` and `grow(w)` flex on both axes.** `growWidth()`/`growHeight()` are
  the single-axis forms. `filler()` flexes on the layout axis of the box it sits
  in (both axes in a stack or grid); `hfiller`/`vfiller` are directional.

**Natural is the flex basis** (CSS `flex: auto`). A flexing child starts at its
natural (zero without one, as for a filler) and the slack left after every
child's natural or fixed size is shared by flex weight. The parent emits the
coupling `extent == natural + weight * F` from the flex the child *publishes*,
so a size word that cleared the flex leaves no coupling behind. Short of space,
`F` goes negative and the deficit is shared by weight too; each flexer stops at
its `min` or at zero while the others take the rest. The coupling sits far below
the gap drive, so a flexer clamped by its `max` leaves its coupling violated and
the slack flows on to the flexers that can still take it.

**Fixed children overflow; they are not squeezed.** Force-sizing a container
smaller than its fixed content shrinks the container's box, and the tiling's
signed trailing gap goes negative. Flexible children are the way to make content
give. Every stamped extent also has a strong `>= 0` floor, so an over-full
container overflows rather than handing a flexer a negative size.

## Containers

A container stamps each child's band (`flattenConstraints`), adds structural
relations, and republishes an aggregate band:

| Aggregate | Box main axis | Box cross axis; stack and grid, both axes |
| --- | --- | --- |
| `natural` | sum | largest |
| `min` | sum | largest |
| `max` | sum, only if every child has one | largest, only if every child has one |
| `flex` | sum of weights | box: none; stack/grid: largest weight |

- **On a flexing axis the natural is only a flex basis.** It is published for the
  parent's aggregation but never stamped, so a flexible child cannot inflate the
  container. Instead the container publishes as its `min` the floor of its
  content: each child's `min`, else the natural of a child that does not flex
  there, else zero.
- **The `max` rule** exists because a single uncapped child leaves the container
  free to grow, and a cap taken from the capped children alone would squeeze it.
- **An empty container has natural 0**, so it takes no room.
- **The weak 100 default.** A container adds a weak default of 100 on an axis
  only when it does not flex there and no child states a natural, so an axis its
  parent neither sizes nor fills still resolves. The default is a relation, not
  a band field, which is why it is withheld where it would outlast a later
  `fill()`.
- **A box propagates child flex outward on its main axis only.** A `fill()` child
  makes an `hbox` horizontally flexible, but not vertically: across, the box
  takes its extent from its children's naturals or from its parent, and the
  flexing child fills that extent up to its `max`. A cross-flexing child's
  natural still counts toward the box's cross natural, so a box of only filling
  children is as tall (or wide) as their naturals. A widget that should flex
  across says so on the box itself.
- **Stack and grid treat both axes as main** for flex: any flexing child makes
  them flexible on that axis. They emit no coupling; a filling child fills its
  slot or cell.
- **Placement.** A child that holds its own extent (a natural that is not a flex
  basis) keeps it and is not pulled; every other child fills its slot up to its
  `max`. Every container places a child by its gravity: across the axis in a
  box, on both axes in a stack or grid. The default gravity is centre, so a
  short child in an `hbox` sits vertically centred; `setGravity` moves it.
- **The box gap is driven only when some child flexes on the main axis**, so a
  container with nothing to stretch does not fight a parent stretching it.

## Wrappers

`margin` (through `applyPureInset`) mints a new outer box,
grows the band fields by twice the inset, re-tags the band onto the outer box,
and adds required inner/outer edge relations. The inner box keeps no band. So
there is always exactly **one band, on the outermost box**, plus a chain of
inset relations; a later size word replaces that one band and the chain
distributes it inward (`image | margin(10) | fixedSize(100)` gives an 80 image).
Keeping the inner band would contradict a later size word. A wrapper adds no
default of its own.

## Anchors

`Constraints` also carries **anchors**: points on the box beyond its edges,
each `leading + fraction * extent + offset` along that axis (top down on y),
keyed by an anchor key (`widget/anchorkey.h`). A key is typed by axis
(`XAnchorKey`, `YAnchorKey`), so an x key cannot name a y anchor. The only
predefined key is `baselineAnchor` (the first baseline); a user creates more by
constructing a key, and copies name the same anchor.

- **Leaves publish them** with `modifier::setAnchor`. `label` publishes the
  baseline where it draws it:
  the text is centred in its box, so the anchor is half the box height plus half
  the text height less the font's descender. The fraction keeps it right when
  a size word moves the label off its natural height.
- **Size words keep anchors**, and pass-through wrappers (`background`,
  `foreground`, theme or role modifiers) leave the band alone. **`margin`**
  re-expresses each anchor against its outer box: the offset grows by
  `inset * (1 - 2 * fraction)`.
- **Containers publish only anchors they define.** An `hbox`, stack or grid
  drops its children's anchors, and a size boundary's content never reaches
  its band. A `vbox` publishes its first child's baseline, at that child's
  natural height (the child sits at the column's top), when the child holds a
  height of its own.

**`baselineHbox`** is an `hbox` whose vertical axis aligns rather than places
by gravity:

- A child that holds its own height is placed so its baseline meets a shared
  line, or its bottom edge when it publishes no baseline (CSS's rule for an
  inline block). A child without a height of its own, or flexing on y, fills
  the row as in an `hbox`.
- The row's natural height is the deepest ascent plus the deepest descent of
  the aligned children at their naturals, held at the firmest of their
  strengths; a `max` below that is raised to it.
- A row taller than that places the aligned block by the row's own gravity
  (centred by default, `setGravity`'s y running bottom up as everywhere), as if
  the row held the block's height and settled in its slot. The line is
  published as the row's own baseline, so rows nest and a margin around one
  keeps it; a row with no real baseline among its children publishes none.
- The row's gravity is set after the row is built, so the row cannot read it.
  It leaves the gravity-dependent parts in its band instead: the block's
  placement (a `GravityPlacement`) and the gravity part of its baseline (a
  `BandAnchor`'s `perGravity`). Whoever stamps the row's band (its container,
  or the region root) knows its gravity and resolves both; a margin
  re-expresses the gravity part as it does the rest of the anchor.
- The line variable is free but for the alignments, so the placement that pins
  it to the row is required (structure), and each alignment is strong at
  `alignStrength`.

## Guides

A **guide** (`XGuide`, `YGuide` in `widget/guide.h`) is a line that widgets
anywhere in one region align to, across sibling containers and at any depth:
form labels in separate rows, say, whose fields should start in one column.
The guide is a token the user creates and captures into the widgets; copies
name the same line. The `align*` modifiers (`modifier/alignguide.h`) bind an
edge, the centre, the baseline or any anchor of a widget's box to it.

- **Bindings ride the band.** A binding is an anchor-shaped point on the box,
  kept in the band beside the anchors. Size words keep it, `margin`
  re-expresses it against its outer box as it does an anchor, and
  `alignBaseline`/`alignAnchor` read the anchor as it stands where the modifier
  is applied (a widget without it is not bound). Stamping the band turns each
  binding into a point expression in the region's spec, so a binding reaches
  the solve from any depth.
- **A guide settles at the furthest natural point.** The region first solves
  without the guides, sets each guide to the largest of its points' positions
  (rightmost on x, lowest on y), then pulls every point onto it at
  `guideStrength`. A point reaches the guide by the cheapest move in the weak
  lane: a placement by gravity yields before a content natural, so in a form of
  left-placed rows the shorter labels' rows move out and the labels end flush
  at the column.
- **Nothing about a guide is required.** The pull is the weakest strong
  strength, so a stated size beats it; a point that cannot reach the guide
  stays put and the unmet pull is logged as `guide`. A guide bound once, or
  never, changes nothing.
- **A guide is scoped to a region.** It does not cross a size boundary: the
  boundary's content is solved as its own region, where the guide settles
  among that region's points only.

## Size boundaries

`makeWidgetWithSize`, `bin` and `scrollView` are size boundaries: their outward
band comes from outside, never from their content, and the content gets its own
region solved at the size the boundary is assigned. So content size never
crosses into the parent's solve.

- `makeWidgetWithSize` and `bin` publish no band unless size words are applied
  to them.
- `scrollView` publishes its own viewport band: natural 400x800, min 100x100,
  flexing on both axes. Its content size on each axis is the natural of the
  content's band, else the viewport extent held within the band's `min`/`max`.

A boundary that hugs its content (reads the content band outward) would be
possible but is deliberately not offered. A guide does not cross a boundary
either: the content's bindings join the content's own region.

## Strengths

Strengths rank firmness for graceful degradation, not authority (authority is
the band's last-writer-wins). From firmest:

- **required**: structure only: tiling and the signed gap equation, inset edges,
  grid lines, a baseline row's line. Each child edge is tied once and every container's slack rides a
  free gap variable, so these are jointly satisfiable whatever the sizes, and no
  stated value can make the solve infeasible. Lowering them buys nothing and
  would let a stated value tear a row apart instead of overflowing it.
- **strong**, scaled to a fixed precedence, as CSS ranks `min-width` over
  `max-width` over `width`: the region anchor (10), then `min` and the `>= 0`
  extent floor (3), then `max` and a slot's fill cap (2), then a fixed size and
  a widget's own pins (1), with a row's baseline alignment (1.5) between a
  fixed size and a `max`, and the guide pull (0.5) below them all. A losing value yields and overflows. The weights stay
  small because the objective mixes them with the weak(1e-5) flex coupling,
  which a much heavier strong weight would lose to round-off.
- **weak**, in order: content natural (2.0), cross-axis fill (1.0), the 100
  default (0.001), the gap drive (0.0008), stack slot fill (0.0004), flex
  coupling (0.00001).

Content above cross-fill is the shrink-wrap default: a measured leaf keeps its
size rather than stretching to fill. The weak lane is sensitive to ordering, so
weigh any new weak constraint against it.

## Known limitations

- **Large flex-weight ratios.** The flex coupling sits at weak(1e-5), below the
  gap drive, so the slack split is exact only while the weight ratio stays
  modest; beyond a ratio of about 80 the split degrades.
- **Phase 3 is not run.** `getWidthForHeight` is part of the descriptor, but
  regions run only the width and height-for-width solves.
- **Conflict logs name kinds, not widgets.** An unmet-constraint line says
  which kind of value lost and by how much, but not which widget stated it.
- **Container bounds are aggregates.** A container's `min` and `max` are summed
  or maxed from its children, so they beat a fixed size on the container too: a
  flexing container fixed below its content's floor takes the floor.

## Future work

- More anchors: a `vbox` publishing its first child's baseline, a last
  baseline, horizontal anchors, and aligning on them in a column.
- Resizing and limiting the OS window itself from the root band, instead of
  clamping only the layout inside it (ase has no API to set a window's size or
  size limits yet).
