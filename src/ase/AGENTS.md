# ase — agent notes

*Last verified against `55f2e55` (2026-08-15).*

The GPU + platform layer. Concepts are in `readme.md`; project-wide conventions
are in the top-level `docs/`.

## Layout

- `platform.h` — the platform-neutral abstraction (window, render context).
- `src/gl/` — the shared OpenGL implementation.
- `src/glx/`, `src/windows/` — the Linux (GLX) and Windows (WGL) bindings;
  `wglrendercontext`/`wglwindow` are the Windows rendering path.
- `src/dummy/` — the headless backend for tests.

## Notes

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
- The headless (dummy) backend is compiled on every platform (`dummysrcs` in
  `src/ase/meson.build`), so any test or app can select it via
  `ase::makeDummyPlatform()` without opening an OS window. `makeDefaultPlatform()`
  returns the native backend where one exists (GLX/WGL) and the dummy otherwise;
  a dependent that must avoid opening a real window (e.g. `apptest` in
  `src/bqui/meson.build`) keys off the target OS.
- **Offscreen ≠ dummy.** `OffscreenWindow` (backend-agnostic,
  `src/offscreenwindow.cpp`) is the *real* backend rendering into an FBO built
  from a `RenderContext`, shown nowhere — how the real GLX/WGL backend runs
  headless, distinct from the no-GL dummy backend. Its FBO needs the render
  context, which a dependent (bqui) already holds, so the dependent constructs
  the window and calls `Platform::registerRenderWindow` to hand the platform a
  weak reference; `makeWindow`, by contrast, builds the windows that need OS
  handles the platform owns. Either way the real GLX/WGL backends keep **one
  render list** of all their windows (real and offscreen) — the
  `needsRedraw()`/`frame()` surface on `WindowImpl` — which the run loop draws
  uniformly (the dummy loop is frame-callback driven and keeps none). **Event routing is separate and backend-specific**
  (WGL's HWND→window map for `wndProc`, GLX's X windows) and holds real windows
  only: a real window is in both, an offscreen window is in the render list
  alone. `present` for an offscreen window is a no-op via the
  `GlRenderContext::present` base — the seam a future pixel readback hooks into;
  a backend's `present` swaps only its own window type and falls back to the
  base for the rest.
- The root `meson.build` adds MSVC-style flags (`/wd4251`, `/bigobj`, `/UNICODE`)
  for any Windows build; those assume an MSVC-compatible driver, which is why
  clang must be `clang-cl`, not `clang++` (the build notes in the repo-root
  `AGENTS.md` cover this).
