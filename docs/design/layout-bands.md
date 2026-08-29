# Layout model: tagged-band constraints

Status: design agreed, to implement (next round). Supersedes the pure
no-bands direction in `layout-pure-solver.md`; builds on the forward-only
region owner in `pure-solver-restructure.md` (constraints ride the builder,
the firewall solves once and hands the solution down as an explicit build
argument).

## The idea in one paragraph

Size and position are different problems, and only position/flex belongs in the
solver. A widget's *size* is a preference with a clear priority order (exact >
min/max clamp > natural > default); a **band** captures that. But instead of a
separate band struct, the band lives **as named constraints inside the one
constraint set** - each size-defining constraint carries a role (`min`, `max`,
`natural`, `flex`), and the rest are untagged relational constraints. Naming is
what makes override cheap: a modifier *replaces* the constraint of a given role
rather than adding a competing one - so there is no strength competition, no
pile-up, and no over-constraint. Everything is still a plain solver constraint;
the names are a compose-time concept, stripped before the tableau.

## The `Constraints` struct

A builder carries a descriptor (a class the builder holds, like the old
`SizeHint`). Each of its three phase functions returns `AnySignal<Constraints>`:

```
Constraints {
    optional<Constraint> min;      // named / overridable-by-replacement
    optional<Constraint> max;
    optional<Constraint> natural;
    optional<Flex>       flex;      // extent == coeff * F ; coeff readable
    vector<Constraint>   constraints;  // untagged, additive relations
    // named anchors beyond the four edges (baseline, ...) live here too
}
```

- **Named fields (`min`/`max`/`natural`/`flex`)** are the band. A size modifier
  sets its field, *replacing* whatever was there. Same role -> last writer wins
  (this is "override always"); different roles coexist; there is no strength
  ladder to exhaust.
- **`constraints`** is additive - relations accumulate.
- All of it is pluggable straight into the tableau; the names are only for
  override and aggregation.

Why this beats a separate band struct: a guide-deferred size need not collapse
to a loose range - its `min`/`max` simply stay in the tableau as constraints and
the firewall solve pins the exact value. Same representation, two dispositions.

**Foundation as landed.** The named fields carry *values*, not pre-baked
`Constraint`s: `min`/`max`/`natural` are `optional<float>` (the natural pairs its
value with the strength it is held at), and `flex` an `optional<Flex>`. A value
is baked into a solver constraint on the current box only at flatten time. This
is what makes a wrapper's grow-and-retag arithmetic (add `2*inset`) rather than
constraint surgery, and it makes "drop the inner band" fall out for free: a
wrapper just swaps in a new outer box and grows the values, so the old box's band
is never materialised to remove. `constraints` is a `LayoutSpec` (the untagged
constraints plus the read-back variables the solver API needs), since the solver
exposes no variable iteration. Per axis the descriptor is `PureLayout`: a phase-1
`width()` and a phase-2 `heightForWidth(widthSolution)` (`widthForHeight` stubs to
the phase-1 width). Phase 2 takes the whole width `LayoutSolution`, not a scalar:
a container cannot turn its own resolved width into its children's widths without
re-solving, so it forwards the same solution to every child unchanged and each
leaf reads its own resolved width from it (`readObb(widthSolution, box)`),
composing to any depth. The firewall sequences the solves - width first, its
solution feeding phase 2 - so a leaf's content height reflows with its resolved
width.

## Three-phase solve

The descriptor exposes three functions, each returning `Constraints`:

```
width()               -> Constraints        // phase 1: solve width
heightForWidth(w)     -> Constraints        // phase 2: height given resolved width
widthForHeight(h)     -> Constraints        // phase 3: width given resolved height
```

The firewall runs three sequential solves (width -> height-given-width ->
width-given-height). For most widgets phase 3 is a no-op returning the phase-1
width, so it is really width->height with a cheap third pass; it only earns its
keep for aspect-locked / rotated content. Bounded, **not** iterate-to-fixpoint:
a widget that is genuinely both height-for-width *and* width-for-height gets a
one-shot approximation (phase 3 can move a width phase 2 already used), which we
accept and document rather than chase a fixpoint.

## Band shape and anchors

Per axis: `{min, natural, max, flex}` - `min`/`natural`/`max` the intrinsic size
(`natural` = the content/preferred size, distinct from the bounds), `flex` the
grow coefficient. The descriptor also exposes **named anchors beyond the four
edges** (baseline, ...); alignment and relations reference them as ordinary
solver variables (a baseline-align across a row is a constraint over the two
widgets' baseline anchors).

## Filler is a flex coefficient, and it aggregates

`flex` is a band component: "how much this widget partakes as a filler." So the
composition falls out - an `hbox` with a `filler` inside has `flex > 0` and **is
itself a filler** to its parent (the inner content can stretch, so the whole can
stretch). The band's `flex` is the *summary* that rides up; the actual slack
*distribution* is still the container's solve (the shared-`F` / weighted-share
constraint). A filler contributes `extent == coeff * F`, tagged `flex`, with
`coeff` readable for aggregation.

**Aggregation is per-axis and per-alignment:**
- Main axis (an `hbox`'s width): children lay end-to-end, so `min`/`natural`/`max`
  and `flex` all **sum**.
- Cross axis (an `hbox`'s height): children overlap, so `min`/`natural`/`max`
  take the **max** and `flex` is "any child flexes". Baseline alignment computes
  the cross extent from the children's baseline anchors
  (`max(baseline-top) + max(bottom-baseline)`), not a raw max.

Each container+alignment carries its own aggregation formula; it reads children's
bands *and* anchors to produce the parent band. Still closed-form, just richer
than sum/max.

> **Callout - greedy cross-axis flex.** The cross-axis "any child flexes" rule
> means a `vbox` whose *cross* axis is width, holding a row (`hbox`) that contains
> a horizontal `filler`, aggregates a width `flex` and so **becomes a horizontal
> filler in its parent**. A vertical stack thus greedily takes horizontal slack
> because something deep inside it can stretch horizontally. This is intended per
> the aggregation rule, but it is surprising and currently untested; revisit if it
> proves too greedy in practice.

## Modifiers transform the `Constraints` - four patterns

1. **Size-setters** (`fixedSize`/`min*`/`max*`): replace the one named field.
2. **Wrappers / insets** (`margin`, `padding`, `border`, `frame`): the one
   non-trivial pattern - a reusable `insetWrapper(inset)` helper that
   - mints a new outer box,
   - grows the named bands by the inset and **retags them onto the outer box**,
   - offsets the anchors,
   - appends the required inner<->outer relations to `constraints`,
   - **drops (subsumes) the inner box's band** - the inner box becomes purely
     relation-derived.
3. **Content leaves** (`label`, `image`): set `natural` from the measurement.
4. **Relational** (align/gravity, guides): append to `constraints`, pick an anchor.

### The load-bearing invariant

**Exactly one band lives on a widget's current outermost box; every wrapper
subsumes the inner band and adds required relations.** This is what makes nested
wrappers + re-sizing work at any depth. Worked example (width axis), image 100:

- `margin(10)`: outer1 `natural == 120`, image inset 10 -> image 100.
- `size(100)`: replaces outer1's band -> outer1 100 -> image 80.
- `margin(10)`: outer2 `natural == 120`, drops outer1's band (now implied),
  appends outer1<->outer2 relations -> outer2 120, outer1 100, image 80.
- `size(100)`: replaces outer2's band -> outer2 100 -> outer1 80 -> image 60.

General form: `image = size - 2*sum(insets)`. It works because there is always
one band (outermost) plus a chain of *required* inset relations; `size()`
replaces the single band and the chain distributes it inward. If a wrapper
*kept* the inner band instead of subsuming it, a later `size()` would contradict
the stale inner constraint - so the subsume rule is required, not just tidy.
`min`/`max` grow-and-retag through each wrapper too, so a bound set deep is
honored at the outer box at any depth.

## Containers stamp and re-publish

When a widget reaches a container, the container **stamps** each child's band
into anonymous constraints in `constraints` (the child's size is baked - no
longer overridable by name), adds positioning/adjacency relations and the flex
constraints (weighted by each child's `flex` coeff), and **publishes a new
aggregate band** (per its alignment) that is overridable again at the next level.
So the override chain runs up to each container and resets there; below it,
stamped.

## Firewalls solve and propagate

A firewall reads the accumulated constraints off the top builder, runs the
three-phase solve once for its whole domain, and propagates the solution **down
as an explicit build argument** (the forward-only mechanism from
`pure-solver-restructure.md`). The solution does not cross a firewall: a firewall
gets a size, solves its interior, and its own band (computed size-independently)
is what its parent sees.

## Guides resolve at the common ancestor

A guide resolves at the lowest common ancestor of its participants:
- **Container-local** (all participants in one container): resolved there,
  folded into that container's alignment aggregation - bands stay exact.
- **Firewall-spanning** (participants across sibling subtrees): deferred to the
  firewall solve; the affected child's band is a conservative range and the
  firewall pins the exact value. This is the "up-exposure" the earlier design
  deferred. Build the local path first; scope/defer cross-subtree guides.

## Force-sizing and overflow

Force-sizing a container shrinks the container's box; **stamped (fixed) children
keep their size and overflow via the signed gap** - they are not squished. This
matches flexbox (`width:150` around two 100px images overflows). The escape hatch
is already in the model: **flexible** children (`filler` / a `flex` coeff)
respond - force-sizing flows down to them and they shrink/grow. So: fixed
children overflow, flexible children respond, opt-in. Document the standard
"sizing a container does not shrink its fixed children - use flex for that."

## Window / OS limits

The top-level (window content) aggregate band drives the OS window min/max size,
for free - the summary is already there.

## Stamped-size strength (decided)

The strength question decides overflow-vs-squish for rigid content: the strength
at which a stamped size / `natural` and the `min`/`max` bounds sit.

- **`natural` / fixed size**: content strength (a heavy weak-lane pull) for a
  content leaf, **strong** for a fixed size — so rigid content overflows rather
  than silently squishing, with `filler`/`flex` the deliberate way to make it
  give.
- **`min`/`max` bounds: strong, not required.** A strong bound is a firm
  preference that *degrades gracefully*: a min that cannot be met yields and
  overflows, a `min` that ties a `max` settles at a determinate compromise —
  where a `required` bound would raise `arrange::Error`, which `solveLayout`
  catches by returning the *previous* solution, freezing every widget in the
  region. Strong still outranks the weak content/natural pull, so a bound clamps
  content as before; it only yields to the required window anchor or a
  contradicting bound of equal strength. The tradeoff — bounds are firm
  preferences, not hard guarantees, and can tie a `strong` fixed size — is
  accepted, and is the same choice as force-sizing overflowing rather than
  squishing.

Because the bounds are strong, they aggregate up a container (main-axis **sum**,
cross-axis **max**) and bake onto the container box without fighting the region
anchor, and a leaf's SizeHint `min`/`max` bridge into the pure band (as a genuine
floor below / cap above the natural — a bound equal to the natural is already the
natural). Both were deferred while the bounds were required.

**Why content strength sits above the cross-fill.** This is a deliberate
*shrink-wrap-by-default* policy, not a tie-break. Under the solver's weighted-L1
objective there is no tie to break: a content leaf's `natural` at `weak(2)`
competes with the cross-fill pull at `weak(1)`, and `weak(2) > weak(1)`, so
content wins outright and the leaf sizes to its measurement rather than stretching
to fill. The margin is only ~2:1, though, so the weak lane is *fragile* - a new
weak-lane pull added between them could flip the default; weigh any addition to
the weak lane against this ordering.

**Sharp edge - a flexing container floors no min for its fixed content.**
`fixedWidth`/`fixedSize` set `natural` (strong), not `min`. A container that
flexes drops its aggregate `natural`, and since its fixed children contributed
only `natural` (not `min`), it publishes no `min` floor for them. So a parent that
force-sizes a flexing container tighter than its fixed content can under-allocate
it, and the fixed content overflows. Flooring the flex `min` at the fixed basis is
deferred: it must sum only the *non-flexing* children's naturals (a `fill()`
child's natural is a soft flex-basis, not a floor), and the cross-axis rule is
unclear - so it is documented rather than rushed.

## Verification to aim for

- Nested-wrapper resize: `image | margin | size | margin | size` -> the
  `image = size - 2*sum(insets)` result on a single evaluate.
- `hbox({img, img}) | size(smaller)` -> children overflow via a negative signed
  gap; the same with `filler` children -> they shrink to fit.
- Baseline row aggregates cross extent from baseline anchors.
- Aggregated `flex`: a `filler` in an `hbox` makes the `hbox` act as a filler in
  its parent.
