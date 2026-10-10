# Layout model (design proposal)

Status: the add-only **model** below is implemented in #131 (add-only
vocabulary, `Band`/`AxisHint`, `XGuide`/`YGuide` guides, the `ResolvedGuides`
firewall down-channel). What is **not** yet as designed is the **solve
granularity**: the shipped wiring makes every container its own firewall+solve
(fine-grained), whereas this model wants firewalls introduced *sparingly*, with
one solve spanning a whole region so constraints and guides couple across
container levels. The rework to that region-solve is the plan in the
[Rework](#rework-region-solve-2026-08-26) section at the end of this file; the
model above is the target and stands. Dated 2026-08-12 (model), 2026-08-26
(rework). No "verified against" stamp yet.

## Core principle

Constraints are **add-only**. A user only ever *adds* constraints; nothing
loosens or overrides. A child owns its own minimums. This is what dissolves the
override / priority-assignment / constraint-removal problems: there is no
authority contest, so there is nothing to rank or remove.

Loosening is logically impossible while a constraint is present (a weaker
competing bound never defeats a tighter one), so it is simply not offered.

## Sizing vocabulary

No `set*` names (they imply replacement, which no longer exists). Every modifier
is additive and tightens the band.

- Per axis: `widthAtLeast/atMost/exactly`, `heightAtLeast/atMost/exactly`.
- Both axes: `sizeAtLeast`, `sizeAtMost`, `exactSize`.
- Soft: `preferWidth`/`preferHeight` (the natural target), `growWeight` (filler
  weight; two grow=1 children split leftover equally, grow=2 gets double).
- Alignment: `alignLeading/Trailing/Center/Baseline`, `guide(g)`.

Composition is monotonic and order-independent: `widthAtLeast(100)` then
`widthAtLeast(150)` -> 150, either order.

## Forcing something smaller than it needs is a strategy, not an override

- Default is **overflow**, not clip: the child keeps its size and draws past the
  container. Visible-when-misplaced beats invisibly-clipped, and clip is costly.
- `clip()` / `scroll()` add a render node (not a constraint), opt-in.
- `scaleToFit()` is a transform.
- Content degradation (truncate/wrap) = the child offering a smaller min about
  itself. Still add-only, still the child's authority.

Overflow default is the *same edit* as removing the forced squeeze: drop the
required containment cap on a container's trailing edge.

## Strengths = firmness / degradation only, never authority

No override => strengths never encode "who outranks whom". They only rank soft
preferences for graceful degradation:

`required` > `strong` (size bounds) > `medium` (guides) > `weak` (gravity,
natural).

An unsatisfiable *required* conflict (e.g. `widthAtLeast(100)` and
`widthAtMost(50)`) is the signal that a strategy is needed - surface it or apply
a default (clip), never silently squeeze or throw into the void. Default to soft
where a value is a preference so pressure degrades instead of failing.

## Positioning: gravity and guides are constraints

Placement is in the solve, not a post-pass (retires `handleGravity`):

- Gravity = a **weak** positioning constraint (center: midpoints equal; leading:
  edges equal). Constrains position, not size; only bites in surplus / decides
  which way overflow spills.
- Guides = **stronger** positioning constraints that beat default gravity by
  strength (add-only: the stronger soft preference wins, no loosening).

So the solver uses placement as a degree of freedom to satisfy guides, falling
back to gravity where a guide does not reach.

## Determinacy invariant: no free variables

A pure constraint system is underdetermined without defaults. Every box edge
must be reached by at least one weak constraint, or its value is undefined.

- Every widget contributes a weak `natural`-size preference.
- A container backstops any child with no intrinsic size (fill or zero) and pins
  position (tiling + weak gravity).
- **Container contract: leave no child under-constrained.**

## Two passes

- **Band up** - closed-form aggregation (`accumulateSizeHints`), a pure query,
  no solver. Main axis = sum, cross = max; anchors split the band at the
  baseline and max each half.
- **Place down** - the solve, per firewall region. Constraints are regenerated
  each frame from the bands, never edited. "Override" = remap the *reported band*
  upstream of conversion; the constraints are never the target.

## SizeHint shape

Per axis: `AxisHint { Band extent; Anchors anchors; }`.

- `Band { float min, natural, max, grow; }` (add `shrink` later if compression
  needs a second weight). Replaces the current `array<float,3>`.
- `Anchors { optional<float> firstBaseline, lastBaseline; }` - sparse; a metric
  (offset), a function of the main-axis size, never of cross-axis allocation.
- Also carries exposed guides: `{ guideId -> pre-construction constraint }`.

## Guides

`XGuide` / `YGuide` - **distinct concrete value types** (not a template alias),
each wrapping a private `avg::UniqueId`. Copyable and `==`-comparable, no id
getter (opaque token; layout internals reach the id via friend/internal access).
Axis lives in the type (compile-time axis safety: an `XGuide` positions on X, a
`YGuide` on Y). Minted when the user defines the widgets, so identity is stable
and shareable exactly like widget/input identity; maps to a per-solve
`arrange::Variable` internally.

`XGuide` = a guide at an X position (constrains horizontal placement); `YGuide`
= a guide at a Y position (constrains vertical placement). (Note: this is the
axis of the *position*, not a line orientation - avoids the "vertical line vs
vertical axis" ambiguity.)

## Firewall

`LayoutFirewall` = the reusable solve boundary. The root is one (fed the window
size, empty resolved-guide map). Introduce **sparingly** - root always,
`makeWidgetWithSize` near leaves, deliberate scopes (scroll view, reusable
component). Within a firewall, constraints and guides span freely across
container levels.

Interface (same at every level, so nesting composes with no special case):

- **Down (build):** `{ size: Vector2f, resolvedGuides: Map<UniqueId,float>,
  params: BuildParams }`.
- **Up (pre-construction):** `SizeHint` (band + anchors + exposed guides).

Resolution:

- Flows down and accumulates: each firewall passes inward `received guides + its
  own newly-resolved guides`, so a guide resolved at any ancestor is a constant
  at any depth (a root guide threads all the way down).
- Exposure chains up: a guide defined deep but referenced shallower is re-exposed
  in each firewall's SizeHint on the way up.
- Gated by **pre-construction expressibility**: the SizeHint can only express
  guide constraints in pre-construction terms, so any guide it exposes is
  outer-resolvable by construction, and any guide depending on inner content
  cannot be exposed and is therefore necessarily an inner-only free variable.
  No cross-firewall cycle is expressible.
- A guide used only inside a firewall is a free variable the inner solve owns
  (outer-free, inner-determined). Resolved guides are matched by `UniqueId` at
  the builder->Element step.

Implementation status (M6): the down-channel and root-as-firewall are in. The
existing `makeWidgetWithSize` primitive *is* the firewall - no distinct
`LayoutFirewall` type was added; it is formalised by the `ResolvedGuides`
BuildParams entry (`src/bqui/include/bqui/widget/resolvedguides.h`, a
`map<UniqueId,float>`) that every firewall reads, and by `rootFirewallParams()`
seeding the window mount with an empty map. A firewall's solve pins any guide
present in the inherited map to that constant (`guideConstraints` consults the
map and pins strong; it also returns its per-guide line variables so a solve can
read a locally resolved line back out). The map threads to descendants through
ordinary BuildParams inheritance, so a guide an ancestor resolves reaches an
inner firewall across the boundary as a constant (proven end to end by
`Layout.resolvedGuideCrossesFirewallBoundary` and, at the solve layer, by
`guideLayout.resolvedGuideValueCrossesBetweenFirewallSolves`). Because a nested
firewall solves in its own local space, a crossing value only lands on the same
window-space line where the boundary offset is zero on the guide's axis.

Deferred: automatically folding a container's *own* solved guide lines into the
map it hands descendants (the read-back primitive exists, but re-injecting a
solve result into an already-built child firewall's params needs builder-param
plumbing the current pipeline does not offer). The whole up-exposure channel
(exposing a deep-defined guide through each firewall's SizeHint) is also not
started. Both are safe to leave: the pass-through down-channel already carries
every pre-construction-expressible guide, which is the gating rule.

## What Cassowary is for

The general engine where distribution / guides / alignment couple. Closed-form
stays the fast path for plain boxes (and is mandatory for the band-up pass, which
is a pure query with no solver in scope).

## Relationship to current code (Stage-4b) and milestones

Current (in this worktree): `src/bqui/src/widget/constraintbox.cpp` +
`constraintlayout.cpp` + `include/bqui/widget/box.h` (`accumulateSizeHints`) +
`include/bqui/sizehint.h`. Bands are `array<float,3>` read as {min, natural,
max}; fillers are equal-split (`childExtent == stretch`); `boxConstraints` caps
the trailing edge required (forces squeeze); gravity is a `handleGravity`
post-pass.

Suggested milestones (each verifiable on its own):

1. **Overflow default** - drop the required trailing cap in `boxConstraints`,
   keep the weak fill pull. Over-full boxes overflow; fillers still fill.
2. **Add-only vocabulary** - `widthAtLeast/atMost/exactly` (+ height, + size*
   both-axis, + `preferWidth/Height`) as additive modifiers over the size hint.
3. **Band struct** - `array<float,3>` -> `Band{min,natural,max,grow}`; weighted
   fillers (`childExtent == natural + grow*stretch`).
4. **Anchors** - `AxisHint`/`Anchors`, baseline metric on text, split-and-max in
   `accumulateSizeHints`, baseline constraints in the box solve.
5. **Positioning as constraints** - gravity + `XGuide`/`YGuide` guides in the
   solve; retire `handleGravity`.
6. **LayoutFirewall** - the boundary primitive, root-as-firewall, multi-level
   guide resolution. Landed: `makeWidgetWithSize` formalised as the firewall, the
   `ResolvedGuides` down-channel, root-as-firewall, and cross-firewall constant
   pinning. Deferred: auto-export of a container's own solved lines and the
   up-exposure channel (see the Firewall section's implementation status).

## Rework: region-solve (2026-08-26)

The shipped wiring made every container its own `makeWidgetWithSize` firewall
with its own solve; nesting is decoupled (band-up aggregation, guide-down
constants). That fine-grained approach was exploratory. This rework replaces the
**wiring** with one solve per firewall **region**, so containers within a region
emit constraints into a shared tableau and constraints/guides couple across
container levels - the model above, realised. Firewalls become **sparse** (root,
`makeWidgetWithSize`/size-dependent construction, scroll views, deliberate
scopes), not per-container.

**Keep** (do not rewrite): the #130 core (`arrange`, `BoxVariables`,
`LayoutSpec`/`LayoutSolution`, `solveLayout`, `readObb`, the constraint
generators) and #131's model layer (`sizevocabulary`, `Band`/`AxisHint`,
`XGuide`/`YGuide`). They already emit *relations*; they just target a shared
solve now. **Rewrite**: `solverLayout` (the per-container firewall+solve) and the
`ResolvedGuides` down-channel (subsumed within a region; it survives only for
crossing a firewall boundary).

### Architecture

- **Region owner:** a `layoutRoot` at each firewall boundary (the window root is
  one) owns the single `withPrevious` solve fold for its region.
- **Up-channel (build):** `Builder::getSizeHint()` becomes `getConstraints() ->
  AnySignal<LayoutSpec>`. Leaves emit band constraints on their own box;
  containers emit parent<->child *relations* + fold children's contributions up;
  a firewall boundary emits only a **band** and stops (interior opaque).
- **Down-channel (placement):** the region's one `LayoutSolution` rides down via
  a `BuildParams` tag (`LayoutSolutionTag`); each widget `readObb`s its own box.
  The build-order forward reference (solution consumed during build, produced by
  it) is broken with `makeInput` + a deferred `handle.set(solution)` - the trick
  the window already uses for its size (`input.h`).
- **Coordinate space:** solve in region-absolute space; convert to
  parent-relative at placement (container does it, like `toObbs`).
- **Firewall boundary (`makeWidgetWithSize`):** reports an honest min/ideal/max
  **band** up, receives an assigned size, runs its own interior region solve.
  Cross-region opacity is structural - a per-region id registry; a ref to an id
  not in the region is a build error (fail loud).

### The solve is change-gated, not per-frame

The solve is a `withPrevious` fold in the signal graph, so it re-runs only when
the constraints signal actually emits. `.check()` the constraints signal (so an
identical spec does not re-fire) and the on-demand frame model does the rest: the
solve fires on constraint *changes*, never per frame. So a full `reset()`+solve
per change is acceptable, and **incremental `setConstraints` is a later
optimisation** (for a large, partially-changing constraint set), not a
prerequisite. The `.check()` on the constraints is the actual gate and must be
kept. (If incremental is added later, mind `arrange`'s id-only diff: a constraint
that keeps its id but changes coefficients is silently dropped, so a changed
constraint must re-mint under a new id.)

### Staged migration (each independently green; reversible until B)

- **Stage A - plumbing, no-op (flagged).** Stand up the region owner +
  `LayoutSolutionTag` down-channel + `getConstraints` up-channel, but containers
  still emit their *current* fragments - now anchored into one shared region
  solve instead of per-container solves. Keep the old per-container path behind a
  flag as an **oracle**. Success = identical geometry through the new substrate.
- **Stage B - the core switch (irreversible).** Containers emit *relations only*;
  leaves emit their bands; delete the per-container `makeWidgetWithSize`+solve and
  the SizeHint *aggregation* (SizeHint survives only as the firewall-boundary
  band). Cross-container-level constraints/guides within a region now work.
- **Stage C - honest, sparse firewalls.** `makeWidgetWithSize` becomes the real
  firewall (band up / size down / interior solve), used sparingly; cross-region
  opacity enforced via the per-region id registry.
- **Stage D - the payoff.** The guide **up-exposure** channel (deep-defined guide
  exposed shallower, solved on the outer firewall) + the cross-container reference
  API (`layoutRef()`/`alignLeft(name)`/`matchWidth`/`baseline`, thin over the
  shared tableau, default *strong* so a conflict yields rather than freezes) +
  text reflow (two-phase, in-fold via `suggestValue`).

### Cross-cutting

- **Region-wide infeasibility:** one bad constraint now freezes the *whole
  region* (vs a per-container solve catching its own). Mitigate: catch-and-hold at
  the region root (degrade to stale geometry, never collapsed), `setConstraints`
  rollback-on-throw, non-required author strengths by default.
- **Reorder fragility:** the "unbounded objective on pure reorder" behaviour -
  harden `arrange`'s reorder path; the reset()+full-solve oracle catches
  regressions.
- **Determinacy invariant** stays: every edge reached by at least one weak
  constraint; a container backstops any sizeless child.

### Verification

Leverage the headless/remote system throughout: drive `inspectorapp` (or a
solver-driven demo) over the inspector protocol and compare `window.renderTree`
geometry from the old per-container path against the new region-solve path (they
must match in Stage A), and assert cross-container guide/alignment behaviour in
later stages the same way. This is the fast introspection loop the model was
meant to be built against.

### First step (Stage A)

1. Add `LayoutSolutionTag` (a `BuildParams` entry) + a `layoutRoot` owning the
   region `withPrevious` fold, mounted at the window and seeded like
   `rootFirewallParams()`.
2. Route existing container specs into that one anchored region solve, behind a
   `regionSolve` flag; keep the per-container path as the oracle. Ensure the
   constraints signal is `.check()`ed.
3. Verify: `layouttest`/`guidelayouttest` reproduce identical geometry through
   the region solve; add a deep-nesting scale case; confirm the same geometry
   over `window.renderTree` via the remote.
