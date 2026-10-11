# Layout: the pure-solver model

*Last verified against `83fbd6f4` (2026-10-11).*

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
of its own, which the next size word can override again. Only `min` and `max`
are rigid; a natural is a weak pull and a flex a share of the slack. A
**region** reads the band off its top builder, fills its assigned rectangle
with the outermost box within that box's own bounds, and solves the whole
subtree once. The solution is handed down into the build, and
each container reads its children's boxes out of it.

## The descriptor

`PureLayout` has three phase functions, each returning `AnySignal<Constraints>`:
`getWidth`, `getHeightForWidth(widthSolution)` and
`getWidthForHeight(heightSolution)`. Phases 2 and 3 receive the whole solution
of the other axis, not a scalar, because a container cannot turn its own width
into its children's widths without solving. It forwards the same solution to
every child, and each leaf reads its own box from it. So a leaf's height can
reflow with its resolved width at any depth.

`Constraints` holds the band fields as plain *values*, not as baked
constraints. A value becomes a constraint
only when it is flattened (`flattenConstraints`) against the box it ends up on.
That is what lets a wrapper grow a band by arithmetic and re-tag it onto a new
box. `relations` is additive: tiling, inset relations, flex couplings, and the
read-back variables the solver API needs (it cannot enumerate variables).

A builder is minted with an empty `PureLayout`. Leaves publish their own:
`label` publishes its measured text extents as a fixed size, `textEdit` and
`scrollView` a default size, and `makeBuilderFromElement` its element size.
`simplePureLayout` builds the common shape (a width band plus a height-for-width
function; phase 3 returns the phase-1 width).

## Regions

**One region is one solve.** `buildPureRegion` reads the top builder's
`PureLayout` and flattens its relations and its `min` and `max` (not its
natural or flex) onto the outermost box. It pins that box's leading edges to
the assigned rectangle's top-left and pulls its trailing edges to the far
corner at medium strength, so the box fills the rectangle within its own
bounds. It solves width, then height given the width solution, combines the
two, and builds the content at the root's clamped size, top-left, with the
combined solution as an explicit build argument. The solution is not a `BuildParams` value, because
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
  natural leaves the size alone. The region is solved at the window's size and
  the clamp happens inside that solve: the root overflows a window below its
  min and leaves a margin right and below in one above its max.
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
  naming their kind (`region`, `min`, `max`, `pin`, `align`, `guide`) and the solved value
  against the stated one, only when that set changes.

## Sizing

Size words (`modifier/constraintsize.h`) each set band fields on one or both
axes, and **only `min` and `max` are rigid**:

- **`fixedWidth`/`fixedHeight`/`fixedSize`** set `min = max = natural` and
  clear `flex`, so a later `fill()` cannot stretch the widget.
- **`minWidth`/`maxWidth`/...** set one bound and drag the other along when
  they cross it, so the last writer wins: `fixedWidth(300) | maxWidth(200)` is
  200, and `maxWidth(200) | fixedWidth(300)` is 300. They leave `flex` alone and
  bound a grower's stretch. A band holds `min > max` only when aggregation puts
  it there, and then the min wins.
- **`defaultSize`** sets the natural: a pull at content strength, felt only
  where there is slack. It is never a cap and never a floor.
- **`fill()` and `grow(w)` flex on both axes;** `growWidth()`/`growHeight()`
  are the single-axis forms.
- **Fillers.** `filler()` (and `hwfiller()`) is an empty band on both axes.
  `hfiller()` is an empty width and a natural height of 0; `vfiller()` is the
  mirror. The zero is only a pull, so an `hfiller` does not cap a row's height,
  and in a column it stays at zero height.

**What takes the slack along a box.** A child is one of three kinds there:

- **Holds a natural:** a natural and no flex. The natural is stamped at content
  strength.
- **Free:** bounds but no natural and no flex. Nothing pulls it, so within its
  bounds it takes slack at no cost.
- **Grower:** an explicit flex, or an empty band (weight 1). The box couples it
  as `extent == basis + weight * F` (the basis is its natural, zero without
  one), so growers share the slack by weight, and pulls the extent to its basis
  at weak(1e-4).

The trailing gap is pulled to zero at weak(0.0008), stronger than the basis
pull. So slack goes first to the free children up to their max, then to the
growers by weight, and what is left stays in the gap. Short of space the order
reverses: the growers give up their share, then a free child shrinks to its
min, and a child holding a natural keeps it while the row overflows its end.
For example, at 500 wide, `hbox(w1[min 50, max 200], w2[natural 100],
filler())` is 200 / 100 / 200; at 220 it is 120 / 100 / 0.

- **A box's growth is priced once.** A box's band along its own axis is marked
  `aggregate`: its children and its gap already price its growth, so its parent
  adds no basis pull there, and a grower costs the same at any depth.
- **The gap is driven whenever a child takes slack** (a free child or a
  grower). When every child holds its own extent the drive is left out, since
  it would only tie with the parent's gap over a box stretched by `fill()`.
- **A flexer clamped by its `max`** leaves its coupling violated, and the slack
  flows on to the growers that can still take it.

**Fixed children overflow; they are not squeezed.** Force-sizing a container
smaller than its fixed content shrinks the container's box, and the tiling's
signed trailing gap goes negative. Every stamped extent also has a strong
`>= 0` floor, so an over-full container overflows rather than handing a
grower a negative size.

## Containers

A container stamps each child's band (`flattenConstraints`), adds structural
relations, and republishes an aggregate band by two rules, one for each
direction relative to how its children are arranged:

| | Along (children tile) | Across (children overlap) |
| --- | --- | --- |
| `min` | sum | largest |
| `max` | sum; uncapped if any child is | smallest cap; uncapped only if no child is capped |
| `flex` | if any child grows; weights sum; none if every child is empty | if any child flexes; smallest weight |
| `natural` | sum of each child's natural (or its min), if any child has one | largest, clamped into `[min, max]` |

- **A box** takes the along rule on its main axis and the across rule on its
  cross axis. **A stack** overlaps its children on both axes, so it takes the
  across rule on both. **A grid** takes the across rule over the cells in each
  column (row), and the along rule over its occupied columns (rows); its tracks
  are held equal, so its natural and min are the largest track's times the
  track count, while maxes and weights sum. A cell spanning several tracks is
  spread evenly over them (extents divide by the span, the weight is kept
  whole). An empty track caps nothing.
- **Stretching across.** A container stretches across if any child flexes
  there, bounded by the smallest max, so a rigid label caps its row's height
  and a fixed 250-wide button caps its column's width. An empty child fills
  whatever it is given and adds nothing.
- **The min beats the max.** When the largest min exceeds the smallest cap, the
  container's max is lifted to the min. A child capped below the container's
  extent keeps its size and is placed by its gravity.
- **One child, one band.** A container adds nothing of its own: with a single
  child it publishes exactly the child's band.
- **An empty container has natural 0**, so it takes no room.
- **Couplings.** A box emits the flex coupling and the basis pull on its main
  axis only; across, a growing child fills the cross extent. Stack and grid
  emit no coupling; a filling child fills its slot or cell.
- **Placement.** A child that holds its own extent (a natural without a flex)
  keeps it and is not pulled; every other child fills its slot up to its
  `max`. Every container places a child by its gravity: across the axis in a
  box, on both axes in a stack or grid. The default gravity is centre, so a
  short child in an `hbox` sits vertically centred; `setGravity` moves it.

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
  the aligned children at their naturals; a `max` below that is raised to it.
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
  grid lines, a baseline row's line. Each child edge is tied once and every
  container's slack rides a free gap variable, so these are jointly satisfiable
  whatever the sizes, and no stated value can make the solve infeasible.
- **strong**, scaled to a fixed precedence: the region's leading pin (10), then
  `min` and the `>= 0` extent floor (3), then `max` and a slot's fill cap (2),
  a row's baseline alignment (1.5), a widget's own pins (1), and the guide
  pull (0.5). A losing value yields and overflows. The weights stay small
  because the objective mixes them with the weak(1e-5) flex coupling, which a
  much heavier strong weight would lose to round-off.
- **medium**: the region fill. It beats every weak pull and loses to the
  root's bounds, and it is not reported as unmet when a bound holds the root
  off the window.
- **weak**, in order: content natural (2.0), cross-axis fill and gravity
  placement (1.0), the gap drive (0.0008), stack slot fill (0.0004), the grower
  basis pull (0.0001), flex coupling (0.00001).

The ladder makes the slack order: a free child pays nothing, a grower pays the
basis pull, the gap pays more, and a natural far more, so the slack goes to them
in that order and is taken back in reverse. The coupling is the weakest, so it
decides only how the growers share. Weigh any new weak constraint against this
order.

## Known limitations

- **Large flex-weight ratios.** The flex coupling sits at weak(1e-5), below the
  gap drive, so the slack split is exact only while the weight ratio stays
  modest; beyond a ratio of about 80 the split degrades. A total grow weight
  below about 0.125 also prices the growers above the gap.
- **Ties.** Two free children, or two naturals squeezed together, cost the same
  per unit, so the solve picks a vertex between them.
- **A `fill()` on a box beside a filler.** The box's growth goes to its own
  gap, which costs more than a filler's basis pull, so the filler takes the
  slack.
- **Phase 3 is not run.** `getWidthForHeight` is part of the descriptor, but
  regions run only the width and height-for-width solves.
- **Conflict logs name kinds, not widgets.** An unmet-constraint line says
  which kind of value lost and by how much, but not which widget stated it.

## Future work

- More anchors: a `vbox` publishing its first child's baseline, a last
  baseline, horizontal anchors, and aligning on them in a column.
- Resizing and limiting the OS window itself from the root band, instead of
  clamping only the root inside it (ase has no API to set a window's size or
  size limits yet).
- Word wrap for labels, which are rigid at their measured text for now.
