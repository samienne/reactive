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
  `ase::makeDummyPlatform(loop)` without opening an OS window. `makeDefaultPlatform(loop)`
  returns the native backend where one exists (GLX/WGL) and the dummy otherwise;
  a dependent that must avoid opening a real window (e.g. `apptest` in
  `src/bqui/meson.build`) keys off the target OS.
- **The platform does not own its run loop.** The `btl::RunLoop` is created by
  the caller (bqui's `App`, a test) and injected at construction; `PlatformImpl`
  holds it by reference and `runLoop()` returns it. The loop must outlive the
  platform, so callers keep it as a named local declared before the platform
  (`RunLoop` is non-movable, so a plain local is the natural home). `App`
  registers its as the process default (`RunLoop::makeDefault()`) so socket IO
  can reach it via `RunLoop::getDefault()`.
- **Offscreen ≠ dummy.** `Platform::makeWindow(context, size, headless)` with
  `headless` gives an `OffscreenWindow` (backend-agnostic,
  `src/offscreenwindow.cpp`): the *real* backend rendering into an FBO built from
  the `RenderContext`, shown nowhere. This is how the real GLX/WGL backend runs
  headless, distinct from the no-GL dummy backend; the one entry point makes an
  on-screen window when `headless` is false. The real GLX/WGL backends keep **one
  render list** of all their windows (real and offscreen) — the render-polling
  surface the `Session` drives, `needsRedraw()`/`frame()`, now **private virtuals
  on `WindowImpl` reached through `friend class Session`**, not part of the public
  window API — which the Session draws uniformly (the dummy registers no drawable
  surface and keeps none). **Event routing is separate and backend-specific**
  (WGL's HWND→window map for `wndProc`, GLX's X windows) and holds real windows
  only: a real window is in both, an offscreen window is in the render list
  alone. `present` is a **surface operation, not a render command**: a
  status-returning `WindowImpl::present(Dispatched)` virtual each window type
  implements (GlxWindow swaps its GLX drawable with the X lock held inside,
  WglWindow `SwapBuffers`es its `hdc_`, OffscreenWindow is the no-op a future
  pixel readback hooks into). The caller enqueues it behind the frame's
  submitted draws through the GL-free `RenderQueue::present(Window&)` seam, which
  forwards to the queue's own dispatcher — draws and present share one FIFO, so
  ordering holds. `PresentStatus` is `Ok`-only on GL; it exists so a backend
  whose present can fail (a lost swapchain) is not a `void`-return retrofit.
- The root `meson.build` adds MSVC-style flags (`/wd4251`, `/bigobj`, `/UNICODE`)
  for any Windows build; those assume an MSVC-compatible driver, which is why
  clang must be `clang-cl`, not `clang++` (the build notes in the repo-root
  `AGENTS.md` cover this).
