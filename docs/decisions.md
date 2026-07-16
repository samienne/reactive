# Decisions

*Last verified against `1c71f09` (2026-07-15).*

Why non-obvious choices were made, so they are not re-litigated. Newest first.
Each entry is intentionally short: the decision and its rationale.

## `SignalContext` holds N signals over one shared `DataContext`

`SignalContext` is parameterized over signal *types* (`SignalContext<TSignals...>`,
each a `Signal<...>`/`AnySignal<...>`), not value packs, and drives all of them
over a single shared `DataContext`. Every entry is addressed by index —
`evaluate<I>()`, `didChange<I>()`, plus `didAnyChange()` — even with a single
signal; there is no non-indexed convenience accessor.

**Why:** several top-level signals (widget instance, introspection, window
title) must advance in lockstep and dedup their shared sub-graphs. One shared
`DataContext` makes `.share()` the cross-signal dedup lever — a shared node
reachable from two entries is evaluated once per pass, not once per entry —
while `.cache()` remains the temporal/sub-expression memoization lever.

Parameterizing over signal *types* (rather than value packs) lets typed and
type-erased signals coexist in one context and keeps `SignalContext` distinct
from `merge`/`combine`, which fuse signals into one value pack. Per-signal
**cadence was deliberately dropped**: every entry updates every frame, and
per-signal evaluation memoization is the user's explicit choice via `.cache()`.

Each entry's result is **cached by owned value**, uniformly regardless of
arity — evaluated on construction and refreshed after any update in which that
entry changed. `evaluate<I>()` returns a reference into that cached
`SignalResult`; the caller extracts a value with `.get<N>()`. Caching by owned
value (never by the reference-holding result the signal evaluates to) is the
deliberate choice: an evaluated result can hold element references into signal
data that go stale after `swapFrameData()` or a move, so the context copies the
result into owned values on the way in and never aliases signal data.

The one hard correctness constraint: `dataContext_.swapFrameData()` runs
**exactly once per update pass, after every entry has updated** — never per
entry, or one entry would rotate away frame data another still needs.

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
Cross-cutting model, conventions, style, and decisions → `docs/`, with a thin
hub in `AGENTS.md` (imported by `CLAUDE.md` so Claude Code auto-loads it).
`AGENTS.md` is the canonical, tool-neutral name; `CLAUDE.md` is a one-line
pointer for Claude Code specifically.

**Why:** one home per fact minimises drift. Reference docs co-located with
declarations are updated in the same diff; narrative and model docs never
transcribe signatures (the thing that rotted the previous docs). Docs may point
at code; code never points at docs.
