# Pure-solver layout: forward-only restructure

Status: implemented. Removed the `makeInput`/`tee` loop, the fragment collectors,
and the `BuildParams` layout down-channels; the region solution is now delivered
as an explicit build argument. Also fixed the nested cross-fill collapse and the
nested main-axis under-determination.

Verified against: `src/bqui/src/widget/constraintbox.cpp`,
`src/bqui/include/bqui/widget/builder.h`, `puresolverlayouttest.cpp` (2026-08).

## Why

The old pure region owner (`pureRegionRootImpl`) had a build-order loop: each
container emitted its constraints into a shared *collector* **while its element
built**, and the solved solution was fed back **down** through the
`LayoutSolutionTag` / `LayoutWidthSolutionTag` `BuildParams` channels, the cycle
broken with `makeInput` + `.tee`. Two consequences:

- **Settle-behind:** a single evaluate (an app's first frame, or any non-update
  path) rendered against the empty input seed -> everything collapsed at the
  origin, converging only after >=1 update pass.
- A separate cross-fill bug made the *converged* layout wrong too.

The loop was unnecessary. The `widget -> builder -> element` chain already lets a
builder report its constraints **without building its element** (exactly how the
banded path reads `SizeHint` off builders in `solverLayout`). So the constraints
now ride the builders, the whole firewall domain is solved **once** up front, and
the solution is handed into the build. Strictly forward.

## Why the delivery had to be a build argument (not BuildParams)

The obvious "solve, then inject the real solution into the child `BuildParams`"
does not work in one build: a container's build reads its params via
`provideBuildParams()`, which resolves against the params captured at
**widget->builder** time -- necessarily *before* the solution (derived from the
composed constraints) exists. `setBuildParams` only rewrites the finished
element's params, after the build logic has already run. So a pre-solved solution
cannot reach a nested container through `BuildParams` without a mutable seam (the
tee) or a second build (which re-mints the box variables and breaks identity).
The fix is to widen the build interface so the solution arrives as an explicit
argument captured **after** the solve.

## Target architecture (as built)

There is no data-structure tree of builders/elements -- it is functional: a
widget yields one builder, a builder yields one element, and a builder's build
*calls* the inner builders. "Compose up / pass down" is signal composition and
direct argument passing.

1. **Pure constraints live on the builder, composed up.** A builder carries an
   optional `PureLayout { horizontal, vertical }` field (each an
   `AnySignal<vector<LayoutSpec>>`), read via `getPureLayout()` before the
   element is built, mirroring `sizeHint_` / `box_`. The pure size modifiers and
   `filler()` accumulate onto it (`addPureConstraint`); a container composes its
   children's `PureLayout` up (`join(array.map(getPureLayout))`) with its own
   relations -- the pure analogue of `accumulateSizeHints`. The field is carried
   across every builder-minting site (`setSizeHint`, `setBuildParams`, type
   erasure, the element-modifier junction) alongside the box variables.

2. **The build interface carries the solution.** A builder's build is
   `(BuildParams, size, solution)`. The type-erased `BuilderBase` stores that
   three-argument shape; an ordinary two-argument build (every leaf and modifier)
   is adapted at construction to be called with exactly two arguments, so the
   solution is dropped for it -- no widget outside the pure containers is edited.
   `operator()(size)` still works, defaulting an empty solution. Detection is by
   whether a build accepts the solution and *not* the two-argument call, tested
   lazily so a `bindArguments`-bound build is never instantiated against the
   solution.

3. **The region owner solves once, forward.** `pureRegionRootImpl` turns the
   content into a builder (no element), reads the composed constraints off it,
   anchors the domain's outermost box to the window, runs the two disjoint
   per-axis solves, combines them, and hands the resulting `solution` signal
   straight into the build. A pure container's build projects each child's obb
   out of the solution, places the children, and passes the **same** solution on
   to a nested container's build. The element is placed against a real solution
   on the first evaluate -- no seed, no tee, no settling pass.

4. **Removed:** the collectors (`RegionVerticalCollectorTag`, the pure
   `SharedVector` plumbing), the `LayoutWidthSolutionTag` down-channel, and the
   `makeInput`/`.tee` at the pure region root. The window root is handed the real
   window size directly. (`RegionCollectorTag` / `LayoutSolutionTag` remain: the
   separate **banded** `regionRoot` still uses them.)

## Cross-fill and nested main-axis extent

Two determinism fixes in the container's own relations:

- **Cross-fill.** `pureAxisConstraints`' cross-axis branch adds
  `child.trail == container.trail | weak(1.0)` (above the weak-100 default, below
  a strong fixed size or required bound), so a nested container stretches to its
  parent's cross extent instead of collapsing to 100.
- **Self size default.** A container adds a weak size default on its own box on
  each axis, so a nested container an axis of which its parent neither sizes nor
  fills (a row's height inside a column) still resolves to a definite extent. Its
  parent's cross-fill and the window anchor both outrank it.

## Strength encoding (unchanged, from the solver sweep)

fixed `== v | strong()`; plain/filler-coupling/default `weak(0.001)`; fill-drive
`G == 0 | weak(0.0008)`; gap eq `last.trail + G == container.trail | required`;
signed gap (no `G >= 0`); min/max required inequalities. Cross-fill sits at
`weak(1.0)` (above the 0.001 default, below strong fixed).

## Two-phase note

The two disjoint solves (x, then y, `combineSolutions`) stay. In the shipped
widget path every vertical constraint is width-independent, so the vertical
fragments are a plain signal rather than staged on the x-solution's resolved
width. The width-dependent-height capability still lives in `BoxDescriptor` /
`layoutRegion` for a direct user (and its unit test), which do not go through the
widget path.

## Verification

- The six scenarios (formRow 120/280, toolbar 80/80/160/80, twoFill 100/150/150,
  noImplicit 80/80/100, squeeze 170/60/170, overflow 300/300) and all E0/E1/E3
  `PureSolverLayout` tests stay green.
- `nestedDemoCorrectOnSingleEvaluate`: the nested `vbox({hbox, hbox})` demo is
  correct on a **single** `evaluate<0>()` with no update passes -- the proof the
  settle-behind is gone.
- Build clang-cl authoritative.
