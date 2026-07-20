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
  run while `init()` is still iterating them.
- **Dropping those inputs goes through an arrival rendezvous**, not a "clear
  only if `init()` has finished" flag: such a flag leaves a window where a
  failure and `init()` each conclude the other will clear, and the inputs are
  retained for the lifetime of the combined future. `init()`, the success path
  and the failure path all arrive; the second arrival drops the inputs. Exactly
  two ever arrive — `init()` on every non-throwing pass, and precisely one of
  success and failure, because a failed input reports failure instead of
  readiness and so the counter never reaches one. Both sites arrive *before*
  publishing the result, so a ready combined future implies the inputs are
  already released. Any new combinator built this way needs both the extra
  count and the rendezvous.
- Most of `btl` is header-only template code, which is why the include-dir
  firewall matters here (see `docs/conventions.md`).
