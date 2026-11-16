# ase — agent notes

*Last verified against `a662f42` (2026-08-22); the pixel readback note against
`1318092` (2026-11-16).*

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
  on the injected loop: per dirty window it gates — without blocking — on that
  window's own `canAcquire` backpressure, skipping a saturated window (which a
  freed slot later wakes back through `requestFrame`), renders and presents
  through the context the window carries, and fences on the window's own queue —
  so `run` names no `RenderContext` of its own.
  A backend supplies only what differs through protected virtuals: the static
  cadence via `runConfig()` (`PlatformBase::RunConfig`: just `frameStep`; the
  frame *rate* is per-window — the loop schedules to the earliest of the windows'
  `nextFrameTime`, not a platform cap — and the dummy queue completes fences
  inline), the OS `wakeSource()` (read
  once at loop start), and the live `getRenderWindows()` list (re-read every tick,
  since windows open and close during a run); there is no `Session`. Manual
  driving is a `Platform::pause()` RAII token (`PauseToken`) whose `step(dt)`
  produces one frame off the same callback/render path an auto tick takes;
  "paused" suspends auto-cadence frame *production*, not the loop (events/IO keep
  pumping), and dropping the token resumes the cadence. `step` lives only on the
  token, so a free-running loop cannot be stepped by construction. A step, like
  a tick, skips a due window that has no free in-flight slot, but it remembers
  the skipped windows: `finishStep()` renders those that have a slot by now, on
  the stepped frame's own time and dt, and reports whether any are left. While
  paused, each loop wake (`requestFrame`, which a completing fence calls) also
  runs the callback set with `PauseToken::setWakeCallback`, on the loop thread,
  so the holder knows when to call `finishStep()` again. The platform has one
  such callback, not one per token: setting one replaces the last, and the
  platform holds it weakly, so it ends with the token that set it and a stale
  token cannot clear a newer one. Resuming the cadence forgets the skipped
  windows. The dummy context can stand in for a busy GPU:
  `DummyRenderContext::holdFences()` leaves fences pending until
  `releaseFences()`.
- **`PlatformImpl` is a pure interface; `PlatformBase` holds the shared guts.**
  Mirroring the `WindowImpl`/`WindowBase` split below: `PlatformImpl`
  (`include/ase/platformimpl.h`) is only the public platform virtuals the
  `Platform` handle and its `PauseToken` call (`makeWindow`, `makeRenderContext`,
  `run`, `pauseFrames`/`resumeFrames`/`stepFrame`/`finishStepFrame`/
  `setWakeCallback`, `requestFrame`, `runLoop`) plus the `getImplOfType`
  type-erasure plumbing. Every backend derives instead from
  `PlatformBase` (`include/ase/platformbase.h`, `src/platformbase.cpp`), which
  owns the injected `loop_`, the shared frame-loop body, the pause/step state, and
  the loop-contract *protected* virtuals a backend fills in (`handleEvents`,
  `runConfig`, `wakeSource`, `getRenderWindows`) — none of which are on the public
  interface. `RunConfig` moved here too (`PlatformBase::RunConfig`).
- **`WindowImpl` is a pure interface; `WindowBase` holds the shared guts.**
  `WindowImpl` (`include/ase/windowimpl.h`) is only the public window virtuals plus
  the `getImplOfType` type-erasure plumbing. Every backend window instead derives
  from `WindowBase` (`include/ase/windowbase.h`, `src/windowbase.cpp`), which owns
  the co-owned `RenderContext`, the per-window present backpressure (the
  non-blocking `canAcquire`/`submitFrameFence` + the `WindowPresentSync` fence
  bookkeeping), the loop-contract members (the `frame` private virtual and the
  concrete `nextFrameTime`, both reached through `friend class PlatformBase`), and
  the `GenericWindow genericWindow_` (protected, so backends
  reach it during OS-event translation). The callback setters and event injectors
  that just forward to `genericWindow_` — plus `getSize`/`getScalingFactor` — are
  concrete forwarders on `WindowBase`; a backend overrides only what genuinely
  differs (`present`, framebuffer, visibility, scaling-aware title/requestFrame,
  `frame`). `nextFrameTime` reads `genericWindow_` on `WindowBase` itself, so
  backends do not reimplement it. The `WindowBase` ctor takes `(context, size,
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
  surface the platform's frame loop drives (`nextFrameTime()` for when a window is
  due, and the `frame()` private virtual), reached through `friend class
  PlatformBase` and not part of the public window API — which the loop draws
  uniformly. The **dummy
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
  `hdc_`) run there; OffscreenWindow's present serves pixel readback (below)
  and DummyWindow's is a no-op, and neither names GL types. Because the
  dispatched swap runs after `present()` returns, the GL windows capture a
  `shared_from_this()` keep-alive into the task so the window outlives it.
  `PresentStatus` is `Ok`-only on GL; it exists so a backend whose present can
  fail (a lost swapchain) is not a `void`-return retrofit.
- **Pixel readback is a render command; capture hangs off present.**
  `CommandBuffer::pushReadback` (`ReadbackCommand` in `rendercommand.h`) reads a
  framebuffer's colour FIFO behind the commands before it. Like every non-draw
  command it bounds the GL draw sort, so it sees every draw pushed before it. GL
  runs `glReadPixels` synchronously on the dispatcher thread
  (`GlRenderState::dispatchedRenderQueue`), which stalls the render thread until
  the preceding draws finish but never the loop thread, maps the top-left region
  to GL's bottom-left origin, flips the rows to top first, and calls the
  callback there. Nothing else in ase takes a region (uploads and the viewport
  start at the origin), so the region follows the returned image's top-left
  convention. The dummy queue completes it at once with `Unsupported`;
  `RenderContextImpl::canReadback` reports which a context does. The window side
  lives in `WindowBase`: `captureNextFrame` appends to a list of armed callbacks
  under a mutex that is never held while a callback runs (a callback may re-arm
  from the render thread), and `capturePresent`, called from a capturing
  window's `present()`, counts the present and moves the whole list into one
  readback submitted on the window's main queue. `Painter::presentWindow`
  flushes the draws before `present()` and the platform submits the frame fence
  after the frame callback returns, so the queue order is draws, readback,
  fence. Only `OffscreenWindow` overrides `canCapture()`, to its context's
  `canReadback()`; on-screen windows keep `WindowBase`'s `false` until their
  present submits a readback before the swap. That is why the `OffscreenWindow`
  that `DummyPlatform::makeWindow` builds for `headless` cannot capture. The
  real-GL check is the manual `bquiheadlesstest`; the arming logic is tested in
  `bquitest` over a fake context that reads back synthetic pixels.
- The root `meson.build` adds MSVC-style flags (`/wd4251`, `/bigobj`, `/UNICODE`)
  for any Windows build; those assume an MSVC-compatible driver, which is why
  clang must be `clang-cl`, not `clang++` (the build notes in the repo-root
  `AGENTS.md` cover this).
