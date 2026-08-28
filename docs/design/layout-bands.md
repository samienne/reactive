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

## Open knob: stamped-size strength

The one thing left to pin, and it decides overflow-vs-squish for rigid content:
the strength at which a stamped size / `natural` sits. Lean: a **firm/strong**
preference so rigid content overflows rather than silently squishing, with
`filler`/flex as the deliberate way to make it give. Because `min`/`max` are band
clamps resolved via the tags before the tableau, the old `required`-vs-`required`
region-freeze does not arise. Finalize at stamping time.

## Verification to aim for

- Nested-wrapper resize: `image | margin | size | margin | size` -> the
  `image = size - 2*sum(insets)` result on a single evaluate.
- `hbox({img, img}) | size(smaller)` -> children overflow via a negative signed
  gap; the same with `filler` children -> they shrink to fit.
- Baseline row aggregates cross extent from baseline anchors.
- Aggregated `flex`: a `filler` in an `hbox` makes the `hbox` act as a filler in
  its parent.
