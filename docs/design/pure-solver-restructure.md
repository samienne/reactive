# Pure-solver layout: forward-only model

The pure-solver region resolves an entire firewall domain in one forward pass:
the constraints ride the builders, the domain is solved once up front, and the
solution is handed into the build. There is no solve/build feedback loop.

Verified against: `src/bqui/src/widget/constraintbox.cpp`,
`src/bqui/include/bqui/widget/builder.h`, `puresolverlayouttest.cpp` (2026-08).

## Why the solve is forward-only

The `widget -> builder -> element` chain lets a builder report its constraints
without building its element -- exactly how the banded path reads `SizeHint` off
builders in `solverLayout`. So the constraints ride the builders, the whole
firewall domain is solved once up front, and the combined solution is handed into
the build. A single evaluate places the element against a real solution; no
update pass is needed to settle.

## Why the solution is a build argument (not BuildParams)

A container's build reads its params via `provideBuildParams()`, which resolves
against the params captured at **widget->builder** time -- before the solution
(derived from the composed constraints) exists. `setBuildParams` only rewrites
the finished element's params, after the build logic has already run. So a
solution cannot reach a nested container through `BuildParams` without a mutable
seam or a second build, which would re-mint the box variables and break identity.
The solution therefore arrives as an explicit build argument, captured **after**
the solve.

## Architecture

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
   erasure, the element-modifier and with-size modifier junctions) alongside the
   box variables.

2. **The build interface carries the solution.** A builder's build is
   `(BuildParams, size, solution)`. The type-erased `BuilderBase` stores that
   three-argument shape; an ordinary two-argument build (every leaf and modifier)
   is adapted at construction to be called with exactly two arguments, so the
   solution is dropped for it -- no widget outside the pure containers needs the
   solution. `operator()(size)` still works, defaulting an empty solution.
   Detection is by whether a build accepts the solution and *not* the
   two-argument call, tested lazily so a `bindArguments`-bound build is never
   instantiated against the solution.

3. **The region owner solves once, forward.** `pureRegionRootImpl` turns the
   content into a builder (no element), reads the composed constraints off it,
   anchors the domain's outermost box to the window, runs the two disjoint
   per-axis solves, combines them, and hands the resulting `solution` signal into
   the build. A pure container's build projects each child's obb out of the
   solution, places the children, and passes the **same** solution on to a nested
   container's build.

## Which widgets are pure-aware

Only `box`, `hbox`, `vbox` and `filler` compose constraints into the region
solve. `stack` and `grid` delegate to the non-region build, so inside a pure
region they appear as opaque boxes: they lay out their own contents by the banded
path rather than joining the surrounding region solve.

## Cross-fill and nested main-axis extent

Two determinism points in a container's own relations:

- **Cross-fill.** `pureAxisConstraints`' cross-axis branch adds
  `child.trail == container.trail | weak(1.0)` (above the weak-100 default, below
  a strong fixed size or required bound), so a nested container stretches to its
  parent's cross extent instead of collapsing to 100.
- **Self size default.** A container adds a weak size default on its own box on
  each axis, so a nested container an axis of which its parent neither sizes nor
  fills (a row's height inside a column) still resolves to a definite extent. Its
  parent's cross-fill and the window anchor both outrank it.

## Strength encoding

fixed `== v | strong()`; plain/filler-coupling/default `weak(0.001)`; fill-drive
`G == 0 | weak(0.0008)`; gap eq `last.trail + G == container.trail | required`;
signed gap (no `G >= 0`); min/max required inequalities. Cross-fill sits at
`weak(1.0)` (above the 0.001 default, below strong fixed).

## Two per-axis solves

The region runs two disjoint solves (x, then y, `combineSolutions`), and they are
independent. In the widget path every vertical constraint is width-independent,
so the vertical fragments are a plain signal, not staged on the x-solution's
resolved width. The width-dependent-height capability -- staging pass 2 on pass
1's resolved width -- lives in `BoxDescriptor` / `layoutRegion` for a direct user
(and its unit test); it is not in the widget path today.
