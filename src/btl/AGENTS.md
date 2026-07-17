# btl — agent notes

*Last verified against `d2e8954` (2026-07-13).*

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
- Most of `btl` is header-only template code, which is why the include-dir
  firewall matters here (see `docs/conventions.md`).
