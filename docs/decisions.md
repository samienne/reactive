# Decisions

*Last verified against `77e391f` (2026-07-22).*

Why non-obvious choices were made, so they are not re-litigated. Newest first.
Each entry is intentionally short: the decision and its rationale.

## `App::run()` with no arguments stops at zero windows, not at the first close

`run()` used to stop when *any* window closed; it now runs while a window
remains in the app's own collection. It is not a rule of the loop: `run()` with
no argument builds a default `running` signal from that collection — true while
it holds a window, false when the last one leaves — and passes it to the same
loop `run(running)` drives, which is unchanged and stops when its signal says
so. The default counts only the app's own windows, not those from
`addWindowArray`; a caller who wants to count those, or to outlive an empty
collection, passes a `running` signal of its own.

**Why:** the app owns its windows now, so closing one is a removal and not an
exit — that is what makes a close button, a title bar, and `removeWindow` one
thing. "Stop at the first close" only ever described a single-window app, and it
described it by accident: with one window the two rules agree. Making the default
an ordinary signal rather than a special case in the loop is what lets a caller
replace it: an app that wants the old behaviour passes `run(running)` an
`onClose` that sets the signal false.

**The cost is real and accepted.** A multi-window app that relied on the main
window closing the whole app now has to say so. There is no deprecation path
worth building for it: the call is the same and only the meaning changed, so a
compiler cannot help, and the behaviour it changes to is the one a reader
expects from the name.

## A draw context is memory, nothing more

`avg::DrawContext` can be constructed from a `pmr::memory_resource*` as well as
from an `avg::Painter*`, and holds only the resource.

**Why:** drawing a render tree produces `avg::Drawing`s, `Shape`s and `Path`s,
all of which allocate out of the context and none of which touch a graphics
back end — the painter was only ever asked for its memory. Splitting the
constructor out makes the whole render tree drawable, and its output
assertable, with no `ase::RenderContext` and therefore no window, which is what
lets the unit tests cover drawing at all. The alternative — compiling ase's
`dummy` back end alongside the real one to get a headless `Painter` — buys a
whole headless *platform*, which is worth having on its own terms and is a
separate concern; drawing does not need it.

## bqui's type-erased classes use no `extern template`

`Widget`, `Element`, `WidgetModifier`, `ElementModifier` and `BuilderModifier`
have no `extern template` declaration and no explicit instantiation on any
toolchain. Every translation unit instantiates them implicitly.

**Why:** GCC cannot export them at all (see `docs/conventions.md`), which is why
Linux `Debug` had never linked. Guarding the declarations under GCC only was
tried first, but it leaves a GCC-built library unusable from a clang-compiled
consumer: the consumer honours the declaration and references a symbol the
library cannot emit.

**Why the cost is acceptable — measured, not assumed.** Baseline versus dropped,
alternating on the same runner, two repetitions each: full-rebuild wall clock
moved by −0.5 s (linux gcc-12 Debug and Release, ~249 s), +1.5 s (windows
msvc-17 Debug, ~225 s) and −5 s (windows msvc-17 Release, ~468 s) — every delta
smaller than the spread between repetitions of the *same* variant. Binary size
is flat: Linux Release `libbqui.so` and `bquitest` byte-identical; Windows
`bqui.dll` 4–20 KB *smaller* and `bquitest.exe` 2–7 KB larger, the copy having
moved from the DLL into the consumer. Cross-compiler linking works in both
directions afterwards, which it did not before. Only 26 translation units name
these types at all.

**Not a general rule.** `avg::Animated<T>` still uses `extern template` and
exports correctly, because it never names its own erased specialization.

## Sanitizers run on one dedicated leg, not on every leg

CI has a single `Sanitize` configuration set (`-Db_sanitize=address,undefined`
over `RelWithDebInfo`) built by one Linux leg with clang. The other legs build
unsanitized, exactly as they did before.

**Why not everywhere:** ASan is *not* a superset of the existing coverage. The
`merge` re-entrancy bug fixed in #104 was clean under ASan — nothing was read
after the invalidation and the buffer stayed live — and was caught by clang-cl's
debug iterators instead. Sanitizers and debug-STL checks find different classes
of bug, so turning ASan on everywhere would trade coverage away rather than add
it. Always-on also costs roughly 2x runtime and 2–3x memory per leg, makes every
failure ambiguous between a real regression and a sanitizer interaction, and —
because ASan changes allocation and layout — would leave the shipped
configuration untested.

`RelWithDebInfo` rather than `Debug`: ASan wants optimized code for tolerable
wall-clock while still keeping frame pointers and symbols for a usable stack.
`ASAN_OPTIONS`/`UBSAN_OPTIONS` set `halt_on_error`/`abort_on_error`, because
UBSan's default is to diagnose and *continue*, which would let the leg report
findings and still exit 0.

**It earned its place immediately:** on its first run the leg surfaced a latent
null-pointer dereference in `pmr::monotonic_buffer_resource` — allocating after
an explicit `release()` walks off a null `current_`. UBSan flagged the pointer
arithmetic leading into it; the defect itself is a hard access violation, not a
paper-only diagnosis.

**Why the configurations repeat themselves:** `Debug`, `Release`, `Tracy` and
`Sanitize` each spell out `wrap_mode` rather than inheriting it from a shared
mixin. `lw` configurations do have an `inherits` chain, and the obvious
refactor is a `Common` mixin plus an `AddressUndefined` one that a TSan or
Debug+ASan set could compose with. It does not work as of lw v0.1.2: a
configuration that inherits a mixin does *not* pick up the mixin's `options`.
Restructuring that way configures with `wrap_mode = default` and an empty
`b_sanitize`, i.e. a sanitizer leg that builds uninstrumented and passes. Revisit
when `inherits` merges options.

## Azure Pipelines is retired

`azure-pipelines.yml` is deleted. It was superseded by the GitHub Actions
workflow years ago — `ci-success` is the only required check — and it invoked
`build/meson/meson.py`, so removing the vendored submodule left it referencing a
path that no longer exists. Nothing else in the tree pointed at Azure DevOps.

## meson comes from pip, and the vendored copy is gone

CI installs `lw` from a pinned, SHA-256-verified loomworks release, and gets
meson and ninja from pip with no version pin. The vendored `build/meson`
submodule and the `build/*.sh` wrappers around it are deleted.

**Why:** meson installs trivially from pip and its language is stable enough
that floating costs less than maintaining a pin, so a submodule pinning a whole
meson checkout bought nothing. The scripts it existed for had rotted anyway —
they hardcoded `g++-6` and called `mesonconf.py`, removed from meson years ago —
and `lw` plus the plain `meson setup`/`compile`/`test` sequence covers what they
did. The sanitizer variants they configured (`-Db_sanitize=thread`,
`-Db_sanitize=undefined`) are a `meson setup` flag away, or an `lw`
configuration if they earn a permanent home.

**Not done:** a Linux `Debug` leg. It does not link — the member-template
constructor of `Widget<std::function<AnyBuilder(BuildParams)>>` is left
undefined once the class is declared `extern template`, which only stays hidden
while Release inlines it. Debug coverage runs on macOS until that is fixed.

## Async tests drive completion manually, not with sleeps

Tests in `src/btl/test/asynctest.cpp` whose correctness depends on *ordering*
(a task hasn't finished yet, a cancelled continuation must not run) create their
own promise/future pair with the test-only `makeManualFuture<Ts...>()` helper
(`src/btl/test/manualfuture.h`) and complete it explicitly, instead of running
a real `async()` job and racing a `sleep_for` against the outcome.

**Why:** the sleeps were an ordering *proxy* — "sleep 100 ms so the other thing
finishes first" — which loses the race under CI load. `whenAllCancelOnFail` was
the worst case and the source of the macOS flake. Manual promises make the
ordering explicit and remove the wall-clock waits, so those tests are
deterministic and the btl suite dropped from ~17 s to ~3 s. This needs **no
change to `async()`/`then()`** — the helper mirrors what `async.h` does
internally (control + weak `Promise` + owning `Future`) without the threadpool.

**Follow-up (deferred):** `async.delayed` genuinely tests the `TimedQueue`
timer and must measure elapsed time; `async.perf`, `whenAllFail`, and
`mergeFail` are throughput/concurrency stress tests. Making *those* fast and
deterministic needs `async()`/`then()` to accept an injectable executor with a
controllable virtual clock plus a pump (`handleTimers`/`drain`) — a production
change we chose not to build here.

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
