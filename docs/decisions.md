# Decisions

*Last verified against `d2e8954` (2026-07-13).*

Why non-obvious choices were made, so they are not re-litigated. Newest first.
Each entry is intentionally short: the decision and its rationale.

## Shape transforms are paint-time; layout size is separate

Transforms on a shape (`translate`/`rotate`/`scale`/`transform`) and the
paint-time `.size(size, gravity)` change *what is drawn* and never affect the
widget's layout size — translating a shape 50px does not move its neighbours.
Layout size is a distinct axis, set at the widget level (size-hint modifiers)
or, in future, a dedicated shape-level `.frame()`.

**Why:** the two axes are genuinely independent (how big the widget is vs. how
the shape paints within it), and keeping transforms paint-only makes them
compose freely and lets a shape overflow its bounds (needed for composition).
When `.frame()` lands, composing two shapes will take the **receiver's** frame,
not the operand's — the operand contributes only geometry.

*Status:* `.size()` (paint) exists; `.frame()` (layout) is not yet implemented.

## No non-uniform scale on shapes

`Shape::scale` takes a single uniform factor. Non-uniform (per-axis) scale is
intentionally omitted.

**Why:** the transform chain is meant to stay a clean translate/rotate/scale
(similarity) group that composes and interpolates cleanly; non-uniform scale
forces full affine handling and breaks that. Arbitrary affine — including
non-uniform scaling — is already available via `Shape::transform(Animated<Transform>)`
for the rare case that needs it.

## `rectangle` covers rounded and square; no separate `roundedRectangle`

There is one `rectangle` with a corner-radius parameter (radius 0 = square);
no distinct non-rounded rectangle factory.

**Why:** a single animatable radius means you can animate between square and
rounded. `roundedRectangle`/`capsule` would just be `rectangle` with a radius,
so they are not worth separate factories. SwiftUI-style names were explicitly
not adopted.

## Path/shape clipping is deferred

`modifier::clip` and `avg::ClipNode` clip to a rectangular OBB only. Clipping a
widget to an arbitrary shape (e.g. a rounded rect) is **not** implemented.

**Why:** it needs the polygon engine wired into the render pipeline
(region-based masking in `avg::Drawing`, a shape-aware clip node, and backend
support) — a real feature, not a modifier tweak. Deferred until the easy wins
are done. Note: the *builder-level* `Shape::clip` (geometry intersection of two
shapes) is unrelated and does work.

## clang-cl is a first-class Windows toolchain

Windows CI builds with both MSVC and clang-cl, and the visibility macros export
template instantiations correctly for clang-cl (see `docs/conventions.md`).

**Why:** clang-cl caught real portability issues MSVC silently tolerated
(unexported template symbols), and broadening compiler coverage is cheap on
hosted runners.

## Merge policy: merge commits, no rebase

PRs merge as **merge commits** by default; **squash** only when explicitly
requested; **rebase-merge is disabled** repo-wide.

**Why:** `master`'s first-parent chain should read as meaningful units of work —
one merge commit (or one squash commit) per PR — rather than a linear spread of
a branch's individual commits.

## Documentation lives in three homes

User narrative → `readme.md`. API reference → **Doxygen in the headers**.
Cross-cutting model, conventions, and decisions → `docs/` (this set), with a
thin auto-loaded `CLAUDE.md` hub.

**Why:** one home per fact minimises drift. Reference docs co-located with
declarations are updated in the same diff; narrative and model docs never
transcribe signatures (the thing that rotted the previous docs). Docs may point
at code; code never points at docs.
