# Pure-solver layout (no bands) — experiment plan

Status: local working draft, uncommitted. Exploratory. Not yet part of the
committed design (`docs/design/layout.md`); fold in when we decide to keep it.

Verified against: nothing yet — this is a plan, not an implementation.

## What we're testing

> Do the full solver-based layout where we have the firewalls that work as
> previously discussed. Without any band information whatsoever. Just pure
> solvers but limited to layout contexts separated by the firewalls. Then we can
> experiment with that and do some measurements how well it performs. Also we'd
> then see what the developer surface really looks like and how well that
> translates to UI designs.

Three questions this experiment answers:

1. **Performance** — how the per-context solves scale, and whether the
   always-present weak defaults or a single large context is where it hurts.
2. **Developer surface** — what writing layout as pure constraints actually
   feels like, and how content-size-dies-at-the-firewall reads in practice.
3. **UI-design translation** — how well real designs (form, toolbar, list row)
   express in this vocabulary.

This is deliberately the *radical* version: no `SizeHint`, no band, no
intrinsic-size reporting anywhere. If it's too painful we'll have learned
exactly where, and can add back the minimum that fixes it.

## The model

### Contexts and firewalls

- **One solver per context.** Contexts nest N-deep, each divided from the next
  by a firewall. Constraints never cross a firewall.
- **Root is just the outermost context** — same solver code, no special path.
  It gets assigned the window size and solves its interior, exactly like any
  inner firewall gets assigned a size and solves its interior.
- **Firewall triggers:**
  - root — implicit, outermost;
  - `layoutFirewall` — explicit modifier, size-agnostic ("start a new context
    here on purpose");
  - size-dependent constructions (`makeWidgetWithSize`, scroll) — implicit /
    forced, because they can't build without a concrete assigned size.
- **A firewall is a black box.** It receives an assigned size from its parent's
  solve and solves its interior independently. **No size flows up** (that was the
  band's job). Content size dies at the boundary.

### No bands, and how sizing works instead

- No `SizeHint` / band machinery anywhere. The `accumulateSizeHints`
  closed-form aggregation is gone.
- **Within a context**, a leaf's content size is expressed as *constraints*
  (text emits `width == measured`, etc.) and participates in the shared solve —
  content-driven sizing works fine locally.
- **Across a firewall**, that's invisible. The outer solve sees only the
  constraints written against the firewall's own box. The developer sizes a
  firewalled widget by hand.
- **Default size.** Every box carries weak per-axis `width == 100` /
  `height == 100` at the weakest strength tier. Any real constraint overrides it;
  it only bites on an axis nothing else pinned. This keeps the solver
  well-posed (no free size DOF -> no ambiguous/unbounded solution) and composes
  (fill-width + free-height gets a 100 height without special-casing).
  Add-only: a box is born with the two weak defaults and they're never removed.
- **Overflow, never clip.** If interior content doesn't fit the assigned area it
  overflows; no automatic clipping.

### Strength ordering (correctness-critical)

    required  >  strong (explicit developer size)  >  medium (guides)
              >  weak (gravity / natural)  >  weakest (the 100 default)

The default must sit **strictly weakest**. A same-strength tie makes Cassowary
minimise summed error and hand back an *average* — neither 100 nor the intended
value. Cheap to guarantee, nasty to debug if we don't.

### Containers add relations, not sizes

`hbox` / `vbox` / `grid` emit **relations** (tie adjacent child edges, tops and
bottoms together, whatever the layout means) into the context solve. They do not
detect-and-inject sizes and they read no band. Any axis a container leaves free
falls to the universal weak default. Clean separation: containers state
structure, the default catches free DOFs.

## The descriptor (replaces SizeHint)

The per-box layout handle carries:

- **stable edge / anchor ids** (edges are a subset of anchors) — the solver
  variables for this box's sides and named points;
- **constraint accessors** (below).

Its meaning is the *inverse* of the old `SizeHint`: the old one was a computed
*value* aggregated upward; this is a stable *identity* plus a constraint stream
feeding a solve. It accumulates, it does not aggregate. Eventual rename:
`LayoutHandle` / `BoxHandle` so nobody reads "hint" and expects a value.

**Signal granularity — split, don't wrap.** Anchors / edge-ids are stable
constants; only the constraints are signals:

    struct { Anchors anchors;  /* constraint accessors returning signals */ };
    // NOT AnySignal<{anchors, constraints}>

Reasons: the old whole-thing-signal existed only because SizeHint was a value;
that value is gone. Wrapping stable identity inside a churning signal is waste
and risks re-minting identity on every constraint tweak — and the arrange diff
matches on id (a changed-but-same-id constraint is silently dropped; a re-minted
id is a spurious remove/add). Keeping constraints as their own signal also *is*
the change-gate.

### Multi-phase per-axis constraint API

    getHorizontalConstraints()                       -> AnySignal<vector<Constraint>>
    getVerticalConstraints(AnySignal<float> width)   -> AnySignal<vector<Constraint>>

- **Two disjoint solves.** Horizontal constraints live over x-edges, vertical
  over y-edges. Pass 1 is x-only, pass 2 is y-only; they share no variables —
  only the *value* `width` flows from 1 into 2. Two N-variable solves instead of
  one 2N-variable one; weak defaults and the context frame split per axis.
- **Staging lives in the signal graph.** The `width` is a signal, so pass-2's
  constraints are a signal derived from pass-1's resolved width. When horizontal
  re-solves, the width signal emits, vertical constraints recompute, pass-2
  re-solves — ordinary propagation under the `.check()` gate. The forward-ref
  tee lives under the region owner, invisible to widgets.
- `getHorizontalConstraints()` takes no arg — a box states its own relative
  constraints; the region owner injects the context frame (root x-edges =
  `[0, assignedWidth]`). Same for assigned height bounding pass 2.
- The `width` passed in is the box's **own** resolved width (right - left).
  Cross-box width->height coupling is not expressible in this signature — an
  accepted limitation (vanishingly rare).
- **Aspect ratio validates the split.** `height == width` is naturally a
  vertical (pass-2) constraint referencing the resolved width; the axis
  partition handles the coupled case without a combined solve.

**Direction is per-context.**

- Width-primary (default): `getHorizontalConstraints()` -> `getVerticalConstraints(width)`.
- Height-primary (mirror): `getVerticalConstraints()` -> `getHorizontalConstraints(height)`
  — the same two methods with the arg on the other one.

A context picks one. Never both in one context: forward + reverse means
width->height->width, a cycle that only a fixpoint iteration resolves
(unbounded, where raw solvers get slow/unstable). The pipeline stays acyclic and
bounded. The "horizontal-constraint-for-height" method is simply the reverse
mode's horizontal half, not an additive third phase.

## Solve mechanism (reused from #130 / the region substrate)

- `solveLayout` = a movable `arrange::Solver` threaded through a `withPrevious`
  fold, `.map`ped to a small solution, `.share()`d.
- **Change-gated.** The solve re-runs only when the constraints signal emits
  (`.check()`ed / deduped) — not per frame. The weak defaults are added once at
  a box's birth and sit dormant; steady-state per-frame cost is ~0.
- The **region owner** concatenates per-box constraint fragments into one
  per-context solve, then projects resolved edges back to per-box geometry.
- arrange's diff matches on **id only**: a constraint whose coefficients change
  but keeps its id is dropped — re-mint under a new id when a constraint's value
  changes.

## Relationship to the current #131 state

Base on the region substrate already landed (Stage A.0 / A.1): `layoutRegion`,
the `LayoutSolutionTag` down-channel, `solveLayout`, the region owner, the
`regionSolve` flag. This experiment **replaces the band feed**: strip
`SizeHint` / band, switch leaves and containers to the per-axis constraint
accessors, add the weak defaults, split into the two-phase solve. Keep the
firewall model; add the explicit `layoutFirewall` modifier.

## Staged implementation

- **E0 — descriptor + weak defaults, single combined solve.** Introduce the box
  layout handle (stable anchors + `getHorizontalConstraints` /
  `getVerticalConstraints`) and the universal weak per-axis default. Ignore
  phasing at first — one combined x+y solve — to get pure-constraint leaves,
  `hbox`, and `vbox` laying out with no bands. Prove a stack lays out via pure
  constraints + defaults; headless equivalence where a band-free layout should
  match the existing one.
- **E1 — two-phase per-axis solve.** Split into x-solve then y-solve(width);
  wire the width signal through. Prove staging with a synthetic
  height-as-a-function-of-width leaf (the text-wrap shape) even before real text.
- **E2 — firewalls as contexts + `layoutFirewall`.** Nested contexts;
  root-as-context uniform; assigned-size-down, nothing-up. Prove nesting and
  boundary opacity (interior invisible to parent; default-sized firewall with no
  constraints).
- **E3 — measurement.** Tracy zones around each per-context solve (solve time,
  constraint count, pivot count). Push a pathological case (one big
  firewall-free context, hundreds of widgets, churning constraints). Write 2-3
  representative layouts by hand and assess ergonomics.
- Reverse mode (height-primary) — optional, after E1, low priority.

## What we measure / deliver

- **Perf:** per-context solve time vs widget count; the marginal cost of the
  always-present weak defaults; single-context scaling; re-solve cost on
  constraint churn; two-phase vs single-solve.
- **Developer surface:** a form, a toolbar, a list row expressed purely in
  constraints — ergonomics, how firewall opacity / manual sizing feels, how
  design intent translates.
- **Correctness:** ON-vs-existing equivalence where a band-free layout should
  match; overflow behaviour.

## Open questions / risks

- Cross-box width->height coupling is not expressible (accepted).
- A single constraint coupling x and y *across* boxes forces a combined solve
  (rare; note where it appears).
- Fixpoint (both directions in one context) is deliberately excluded; measure
  only if a real case demands it.
- **Retain-cycle hazard:** anything carried as a `shared_ptr` through
  `BuildParams` forms a cycle through the builder graph and must be weak. The
  Linux **Sanitize** CI leg (LSan) is the ground truth for leaks — local
  clang-cl and non-sanitized legs miss them.
- Region-wide infeasibility and reorder fragility (from the region-solve notes)
  carry over.

## Verification approach

- **Headless / remote:** `inspectorapp` (dummy platform) + `bqui_mcp.py`
  (4-byte length-prefixed JSON-RPC); `window.list` / `introspect` /
  `renderTree`; `realiseConverged` for instance-geometry checks. Wrap any
  headless-app launch in a timeout and kill the process.
- **Build/test:** `lw build` / `lw test` on `Debug:clang-cl-18.1.7` locally;
  build clang-cl before claiming green. CI **Sanitize** leg is the leak/UB
  ground truth.
