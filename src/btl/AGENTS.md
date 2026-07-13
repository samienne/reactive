# btl — agent notes

*Last verified against `d2e8954` (2026-07-13).*

Small utilities used everywhere. Concepts are in `readme.md`; project-wide
conventions are in the top-level `docs/`.

- **`visibility.h` is the root of the symbol-export scheme** (`BTL_EXPORT`,
  `BTL_EXPORT_TEMPLATE`, …) that every library re-exports as `AVG_EXPORT_*`,
  `BQUI_EXPORT_*`, etc. Changing it affects all libraries — see
  `docs/conventions.md` for the MSVC-vs-clang-cl export rules.
- `async.h` / `future/` provide the async primitives. The test
  `async.whenAllCancelOnFail` (`test/asynctest.cpp`) is **timing-flaky on macOS
  CI**; re-run the failed job to clear it.
- Most of `btl` is header-only template code, which is why the include-dir
  firewall matters here (see `docs/conventions.md`).
