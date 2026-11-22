# btl — agent notes

*Last verified against `2c6969c` (2026-07-19); the promise abandonment note
against `84e63727` (2026-11-22).*

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
- **A promise abandoned unfulfilled fails its future** with
  `future::BrokenPromise`. `Promise` copies share one `detail::PromiseState`,
  whose destructor does this, so it fires once, when the last copy goes, and
  only if nothing was set first. Continuations then run inside that
  destructor, on the thread that dropped the last copy, often a teardown path;
  `then` skips its function and passes the failure on, and a detached future
  swallows it. An `onFailure` callback is not wrapped in a `try`, so one that
  throws there terminates. `Future<>::get()` does not rethrow (it never reads
  the result); use `getRefTuple()` or `onFailure` to observe a failure of a
  valueless future.
- Most of `btl` is header-only template code, which is why the include-dir
  firewall matters here (see `docs/conventions.md`).
- **On Windows, posts and timers can run under the message-source callback.**
  DefWindowProc's move/size loop holds that callback for a whole window drag,
  so `src/win32/runloopimpl.cpp` arms a thread timer around it whose callback
  runs due posts and expired timers. It acts only while Windows reports a
  move/size loop (`GUI_INMOVESIZE`), never inside a post or timer callback,
  and never fires sources. Other modal loops, such as a dialog opened from an
  input handler, are not pumped. The check is the thread's state, not the
  stack: a window procedure that opens a nested modal loop (a message box,
  drag-and-drop) while handling a message from inside a move/size loop is
  pumped with that handler on the stack. The first exception from a callback
  run by the pump is rethrown from `run()` when the message-source callback
  returns; it takes precedence over one thrown by that callback itself. After
  that exception the pump runs nothing more for the rest of that callback, so
  posts and timers wait until the drag ends.
