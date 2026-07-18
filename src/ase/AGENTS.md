# ase — agent notes

*Last verified against `77e391f` (2026-07-22).*

The GPU + platform layer. Concepts are in `readme.md`; project-wide conventions
are in the top-level `docs/`.

## Layout

- `platform.h` — the platform-neutral abstraction (window, render context).
- `src/gl/` — the shared OpenGL implementation.
- `src/glx/`, `src/windows/` — the Linux (GLX) and Windows (WGL) bindings;
  `wglrendercontext`/`wglwindow` are the Windows rendering path.
- `src/dummy/` — the headless backend. It opens no OS window and drives a
  deterministic, fixed-`dt`, frame-budgeted loop (`DummyPlatform::run`/`step`),
  so a run is bounded and reproducible. `DummyWindow` delegates callback storage
  and dispatch to the shared `GenericWindow` (same as `WglWindow`/`GlxWindow`).
  It is compiled on **every** platform (see `dummysrcs` in `meson.build`) so any
  app can run headless.
- `GenericWindow` (`src/genericwindow.cpp`) is a shared, backend-internal helper
  holding the window's callbacks and the event-injection logic; every concrete
  window owns one privately. Programmatic input goes through the abstract
  `WindowImpl`/`Window` `inject*` methods (which forward to it), never through
  `GenericWindow` directly — it stays off the public surface.

## Notes

- Every platform's `run()` owns the frame loop and the clock: each iteration it
  samples `std::steady_clock` (the dummy uses a fixed `dt`), builds the `Frame`,
  runs `handleEvents()` and the app frame callback, then calls the clock-agnostic
  `PlatformImpl::step(Frame const&)` — the minimal primitive that just injects the
  frame into the windows (advancing each live `window->frame`). An external driver
  can call `step()` with its own controlled time.
- Two platform factories: `makeDefaultPlatform()` returns the OS backend (WGL/GLX,
  or the dummy where there is none — `dummydefaultplatform.cpp` defines it there),
  and `makeDummyPlatform()` always returns the headless one. `bqui::App::run`
  selects between them via env (`REACTIVE_HEADLESS`/`REACTIVE_PLATFORM=dummy`) or a
  programmatic override; platform (headful/headless) and mode (normal/agentic) are
  orthogonal.

- The Windows GPU/rendering path lives under `src/windows/` and `src/gl/` — this
  is where to look for Windows rendering performance or context-selection issues
  (integrated-vs-discrete GPU, vsync).
- A backend owns its window's lifetime in the `WindowImpl` destructor —
  `GlxWindow` destroys its X window there and `WglWindow` its `HWND`. Leaving
  that out does not fail a test: nothing closes a window while the app runs
  unless the window list says to, so the symptom is a dead window left on
  screen. The WGL backend was missing it until a window list became reactive.
- **Unfixed:** destroying a WGL window mid-run leaves two loose ends that were
  unreachable while windows lived for the whole process, and became reachable
  when the window list did. `WglDispatchedContext` caches the last DC it made
  current and nothing invalidates that entry, so a recycled `HDC` can make it
  skip a `wglMakeCurrent` it needed; and with no window left the run loop keeps
  submitting fences against a context current on a destroyed DC. Releasing the
  context from the window's destructor would close both, and has to be
  dispatched to the render thread.
- `src/ase/meson.build` sets `ase_is_headless`, which is how a dependent decides
  whether a test may open a window (`src/bqui/meson.build` uses it).
- The root `meson.build` adds MSVC-style flags (`/wd4251`, `/bigobj`, `/UNICODE`)
  for any Windows build; those assume an MSVC-compatible driver, which is why
  clang must be `clang-cl`, not `clang++` (the build notes in the repo-root
  `AGENTS.md` cover this).
