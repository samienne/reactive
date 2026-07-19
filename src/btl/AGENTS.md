# btl — agent notes

*Last verified against `2c6969c` (2026-07-19).*

Small utilities used everywhere. Concepts are in `readme.md`; project-wide
conventions are in the top-level `docs/`.

- **`visibility.h` is the root of the symbol-export scheme** (`BTL_EXPORT`,
  `BTL_EXPORT_TEMPLATE`, …) that every library re-exports as `AVG_EXPORT_*`,
  `BQUI_EXPORT_*`, etc. Changing it affects all libraries — see
  `docs/conventions.md` for the MSVC-vs-clang-cl export rules.
- `async.h` / `future/` provide the async primitives. Tests whose correctness
  depends on ordering drive completion manually via `makeManualFuture<Ts...>()`
  (`test/manualfuture.h`) instead of sleeping — see `docs/decisions.md` for the
  rationale and the deferred injectable-executor follow-up. `async.delayed` is
  the one test that still legitimately measures wall-clock time.
- **Combinator `init()` runs re-entrantly.** `merge` (`future/merge.h`) and
  `whenAll` (`future/whenall.h`) register a completion callback per input while
  walking their input container, and `addCallback` invokes the callback *inline*
  when the input is already ready. Their outstanding-input counters therefore
  start one above the input count; `init()` owns that extra count and releases
  it only after the walk, so completion — which destroys the inputs — can never
  run while `init()` is still iterating them. Any new combinator built this way
  needs the same extra count.
- Most of `btl` is header-only template code, which is why the include-dir
  firewall matters here (see `docs/conventions.md`).
