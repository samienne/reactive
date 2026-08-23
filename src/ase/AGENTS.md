# ase — agent notes

*Last verified against `6b21821` (2026-08-18).*

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
  the caller (bqui's `App`, a test) and injected at construction; `PlatformBase`
  holds it by reference and `runLoop()` returns it. The loop must outlive the
  platform, so callers keep it as a named local declared before the platform
  (`RunLoop` is non-movable, so a plain local is the natural home). `App`
  registers its as the process default (`RunLoop::makeDefault()`) so socket IO
  can reach it via `RunLoop::getDefault()`.
- **The frame loop is context-free and lives on the platform.** `Platform::run`
  (`PlatformBase::run`, one shared body in `src/platformbase.cpp`) drives frames
  on the injected loop: per dirty window it gates on that window's own `acquire`
  backpressure, renders and presents through the context the window carries, and
  fences on the window's own queue — so `run` names no `RenderContext` of its own.
  A backend supplies only what differs through protected virtuals: the static
  cadence via `runConfig()` (`PlatformBase::RunConfig`: `frameStep`, the dummy's
  `maxFrames` self-pump budget, and `maxFps` — a wall-clock cap set through
  `PlatformBase::setMaxFps` that headless backends use to pace a loop with no
  vsync; the dummy queue itself completes fences inline), the OS `wakeSource()` (read
  once at loop start), and the live `getRenderWindows()` list (re-read every tick,
  since windows open and close during a run); there is no `Session`. Manual
  driving is a `Platform::pause()` RAII token (`PauseToken`) whose `step(dt)`
  produces one frame off the same callback/render path an auto tick takes;
  "paused" suspends auto-cadence frame *production*, not the loop (events/IO keep
  pumping), and dropping the token resumes the cadence. `step` lives only on the
  token, so a free-running loop cannot be stepped by construction.
- **`PlatformImpl` is a pure interface; `PlatformBase` holds the shared guts.**
  Mirroring the `WindowImpl`/`WindowBase` split below: `PlatformImpl`
  (`include/ase/platformimpl.h`) is only the public platform virtuals the
  `Platform` handle and its `PauseToken` call (`makeWindow`, `makeRenderContext`,
  `run`, `pauseFrames`/`resumeFrames`/`stepFrame`, `requestFrame`, `runLoop`) plus
  the `getImplOfType` type-erasure plumbing. Every backend derives instead from
  `PlatformBase` (`include/ase/platformbase.h`, `src/platformbase.cpp`), which
  owns the injected `loop_`, the shared frame-loop body, the pause/step state, and
  the loop-contract *protected* virtuals a backend fills in (`handleEvents`,
  `runConfig`, `wakeSource`, `getRenderWindows`) — none of which are on the public
  interface. `RunConfig` moved here too (`PlatformBase::RunConfig`).
- **`WindowImpl` is a pure interface; `WindowBase` holds the shared guts.**
  `WindowImpl` (`include/ase/windowimpl.h`) is only the public window virtuals plus
  the `getImplOfType` type-erasure plumbing. Every backend window instead derives
  from `WindowBase` (`include/ase/windowbase.h`, `src/windowbase.cpp`), which owns
  the co-owned `RenderContext`, the per-window present backpressure
  (`acquire`/`submitFrameFence` + the `WindowPresentSync` fence bookkeeping), the
  loop-contract private virtuals (`needsRedraw`/`frame`, with `friend class
  PlatformBase`), and the `GenericWindow genericWindow_` (protected, so backends
  reach it during OS-event translation). The callback setters and event injectors
  that just forward to `genericWindow_` — plus `getSize`/`getScalingFactor` — are
  concrete forwarders on `WindowBase`; a backend overrides only what genuinely
  differs (`present`, framebuffer, visibility, scaling-aware title/requestFrame,
  `needsRedraw`/`frame`). The `WindowBase` ctor takes `(context, size,
  scalingFactor)` so it can build `genericWindow_`. The platform render lists are
  `weak_ptr<WindowBase>` since the loop hooks live there. `getRenderContext` is
  a pure virtual on `WindowImpl` (implemented by `WindowBase`), so `Window`
  reaches the render context without a downcast; the window's main render queue
  is just `getRenderContext().getMainRenderQueue()`, so `Window::getMainRenderQueue`
  is a convenience computed on that and stays off the impl interface.
- **Offscreen ≠ dummy.** `Platform::makeWindow(context, size, headless)` with
  `headless` gives an `OffscreenWindow` (backend-agnostic,
  `src/offscreenwindow.cpp`): the *real* backend rendering into an FBO built from
  the `RenderContext`, shown nowhere. This is how the real GLX/WGL backend runs
  headless, distinct from the no-GL dummy backend; the one entry point makes an
  on-screen window when `headless` is false. The real GLX/WGL backends keep **one
  render list** of all their windows (real and offscreen) — the render-polling
  surface the platform's frame loop drives, `needsRedraw()`/`frame()`, now
  **private virtuals on `WindowBase` reached through `friend class PlatformBase`**,
  not part of the public window API — which the loop draws uniformly. The **dummy
  backend registers its windows on the same render list** and is driven by the one
  loop like the real backends; its `frame()` runs (evaluating the signal graph and
  building the render tree), only the draw and present are no-ops on the dummy
  queue. **Event routing is separate and backend-specific**
  (WGL's HWND→window map for `wndProc`, GLX's X windows) and holds real windows
  only: a real window is in both, an offscreen window is in the render list
  alone. `present` is a **surface operation, not a render command**: the
  backend-agnostic, status-returning `WindowImpl::present()` virtual each window
  type implements. A GL window sequences its own swap by enqueuing it on the
  same GL dispatcher its draws went through (reached via its own
  `getRenderContext().getMainRenderQueue().getImpl<GlRenderQueue>().dispatch(...)`), so the swap
  runs FIFO after the submitted draws on the render thread; `present()` returns
  immediately without blocking. The swap body — the only place the GL-only
  `Dispatched` tag legitimately appears — is what GlxWindow (swaps its GLX
  drawable with the X lock held inside) and WglWindow (`SwapBuffers`es its
  `hdc_`) run there; OffscreenWindow and DummyWindow are the no-op a future
  pixel readback hooks into and name no GL types. Because the dispatched swap
  runs after `present()` returns, the GL windows capture a `shared_from_this()`
  keep-alive into the task so the window outlives it. `PresentStatus` is
  `Ok`-only on GL; it exists so a backend whose present can fail (a lost
  swapchain) is not a `void`-return retrofit.
- The root `meson.build` adds MSVC-style flags (`/wd4251`, `/bigobj`, `/UNICODE`)
  for any Windows build; those assume an MSVC-compatible driver, which is why
  clang must be `clang-cl`, not `clang++` (the build notes in the repo-root
  `AGENTS.md` cover this).
