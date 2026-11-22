# ase — agent notes

*Last verified against `a662f42` (2026-08-22); the pixel readback note against
`5a26b28f` (2026-11-22); the fence and backpressure notes against `f1595378`
(2026-11-22).*

The GPU + platform layer. Concepts are in `readme.md`; project-wide conventions
are in the top-level `docs/`.

## Layout

- `platform.h` — the platform-neutral abstraction (window, render context).
- `src/gl/` — the shared OpenGL implementation.
- `src/glx/`, `src/windows/` — the Linux (GLX) and Windows (WGL) bindings;
  `wglrendercontext`/`wglwindow` are the Windows rendering path.
- `src/dummy/` — the headless backend for tests.
- `test/` — the `asetest` unit tests.

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
  window's own `canAcquire` backpressure, skipping a saturated window (which the
  fence recheck below brings back), renders and presents through the context the
  window carries, and fences on the window's own queue — so `run` names no
  `RenderContext` of its own. A backend supplies only what differs through
  protected virtuals: the static cadence via `runConfig()`
  (`PlatformBase::RunConfig`: just `frameStep`; the frame *rate* is per-window —
  the loop schedules to the earliest of the windows' `nextFrameTime`, not a
  platform cap — and the dummy queue signals fences on submit), the OS
  `wakeSource()` (read once at loop start), and the live `getRenderWindows()`
  list (re-read every tick, since windows open and close during a run); there is
  no `Session`. A wake (`requestFrame`, an OS event, the fence recheck) cuts a
  pending timer short, but never to sooner than one `frameStep` after the last
  tick, so the fence wakes cannot run an animating window past the cadence.
  Nothing wakes the loop when a fence signals: while a window wants a frame but
  has no free slot, a `kFenceRecheckInterval` (1 ms) timer recounts such windows
  and wakes the loop when the count drops; it is not armed otherwise. So a frame
  that quiesces gets no trailing tick, and a headless test that ends on one
  stops the loop from it. A tick's `dt` is its real elapsed time capped at the
  longer of four frame steps and the wait the loop scheduled for a window's
  `nextFrameTime`, so a scheduled update arrives on time. The wait counts in
  full even when a wake ends it early. Time past the cap, such as a stall, is
  dropped; a stall that follows a wake which cut the wait short, but ends before
  the wait's original end, is within the cap and counts in full. A pause ends
  the scheduled wait where the pause starts, so the wait before the pause counts
  and the pause does not. Manual driving is a `Platform::pause()` RAII token
  (`PauseToken`) whose `step(dt)` produces one frame off the same
  callback/render path an auto tick takes; "paused" suspends auto-cadence frame
  *production*, not the loop (events/IO keep pumping), and dropping the token
  resumes the cadence. `step` lives only on the token, so a free-running loop
  cannot be stepped by construction.
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
  `WindowImpl` (`include/ase/windowimpl.h`) is only the public window virtuals
  plus the `getImplOfType` type-erasure plumbing. Every backend window instead
  derives from `WindowBase` (`include/ase/windowbase.h`, `src/windowbase.cpp`),
  which owns the co-owned `RenderContext`, the per-window present backpressure
  (the non-blocking `canAcquire`/`submitFrameFence` over the window's own
  outstanding `Fence`s, loop thread only), the loop-contract members (the
  `frame` private virtual and the concrete `nextFrameTime`, both reached through
  `friend class PlatformBase`), and the `GenericWindow genericWindow_`
  (protected, so backends reach it during OS-event translation). The callback
  setters and event injectors that just forward to `genericWindow_` — plus
  `getSize`/`getScalingFactor` — are concrete forwarders on `WindowBase`; a
  backend overrides only what genuinely differs (`present`, framebuffer,
  visibility, scaling-aware title/requestFrame, `frame`). `nextFrameTime` reads
  `genericWindow_` on `WindowBase` itself, so backends do not reimplement it.
  `GenericWindow` is loop-thread only except `requestFrame`, which just sets an
  atomic flag, and a request made while the frame callback runs outranks the
  delay the callback returns. Every backend's `requestFrame` also wakes its
  platform loop through `PlatformBase::requestFrame`, which is why the dummy and
  offscreen windows hold a platform reference like the GLX/WGL ones. The
  `WindowBase` ctor takes `(context, size, scalingFactor)` so it can build
  `genericWindow_`. The platform render lists are `weak_ptr<WindowBase>` since
  the loop hooks live there. `getRenderContext` is a pure virtual on
  `WindowImpl` (implemented by `WindowBase`), so `Window` reaches the render
  context without a downcast; the window's main render queue is just
  `getRenderContext().getMainRenderQueue()`, so `Window::getMainRenderQueue` is
  a convenience computed on that and stays off the impl interface.
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
  immediately without blocking. The swap body is what GlxWindow (swaps its GLX
  drawable with the X lock held inside) and WglWindow (`SwapBuffers`es its
  `hdc_`) run there; OffscreenWindow and DummyWindow are no-ops and name no GL
  types. Because the dispatched swap runs after `present()` returns, the GL
  windows capture a `shared_from_this()` keep-alive into the task so the window
  outlives it. `PresentStatus` is `Ok`-only on GL; it exists so a backend whose
  present can fail (a lost swapchain) is not a `void`-return retrofit.
- **The GL viewport size travels with the frame.** WglWindow and GlxWindow keep
  the pixel size on the loop thread (set at creation and on WM_SIZE,
  WM_DPICHANGED or ConfigureNotify), and `frame()` first dispatches a task
  carrying it, with the same keep-alive, that sets it on the default
  framebuffer. The FIFO runs it before that frame's draws, so the
  framebuffer's size is render-thread-only state that `makeCurrent` reads
  without a lock. GlxWindow's present task likewise captures the
  `_NET_WM_SYNC_REQUEST` counter value by value.
- **`Fence` is a polled handle.** `CommandBuffer::pushFence()` makes one and
  queues it. The command buffer is backend-agnostic, so every backend's fence
  is a `BasicFence`. `isSignaled()` is an atomic read, and the frame throttle
  uses only that. `whenSignaled()` is the one future entry point. The backend
  calls `signal()`, which hands back the waiting promises, and a per-queue
  `FenceDrain` fulfils them in order from one btl thread-pool task at a time,
  so no continuation runs on the GL render thread or under the dispatcher's
  mutex (where the idle poll runs).
  `~GlRenderState` deletes unsignaled sync objects and drops its idle poll,
  which captured it; those fences never signal, and once their last reference is
  released, on the destroying thread rather than the render thread, their
  futures fail with `btl::future::BrokenPromise`. The exception is a submit
  whose rendering throws before reaching a fence's command: that fence is
  destroyed on the render thread, so its `BrokenPromise` continuation runs
  there. A window co-owns its context and so its queues, so a live window's
  fences cannot lose their queue.
- **Render-thread-only GL functions take the `Dispatched` tag.** Its
  constructor is private and only `Dispatcher` is a friend: the dispatcher
  creates the tag on its thread and passes it to every task and idle callback
  it runs, and the task passes it down (the window tasks above, the render
  state's queue). Code that is not running as a dispatched task cannot make
  one, so a call from the loop thread does not compile. The tag is copyable:
  never store one or capture it into something that runs elsewhere. It
  belongs to the GL backend, the only one with a thread of its own (the
  `GlDispatchedContext` dispatcher), so only GL, GLX and WGL classes take it
  and backend-neutral interfaces stay free of it. A backend-neutral virtual
  such as `present` therefore has no tag; where its dispatched task calls a
  GL-side helper, as GLX's does, the helper takes it.
- **Cursor.** A window's pointer is a `Cursor` value (today only a
  `CursorKind`; an image cursor would be another constructor) stored in
  `GenericWindow`, and `WindowBase::setCursor`/`getCursor` forward to it, so the
  dummy and offscreen windows only store it. It covers the client area only;
  the frame and drag-resize pointers belong to the OS. WGL answers
  `WM_SETCURSOR` for `HTCLIENT` with the mapped `IDC_*` cursor (shared system
  handles, never destroyed) and lets `DefWindowProc` handle the rest;
  `setCursor` also calls `SetCursor` at once when the pointer is in the client
  area or captured by the window, since `WM_SETCURSOR` waits for the next move
  and is not sent during capture; it skips this during a move/size loop, where
  the system holds capture and owns the pointer. GLX loads the theme cursor
  through libXcursor when the optional `xcursor` dependency is found
  (`ASE_HAVE_XCURSOR`), falling back to `XCreateFontCursor`; `hidden` is a
  blank 1x1 pixmap cursor. Cursors are cached per window and freed in
  `destroy()`.
- **Pixel readback is a render command, not a window feature.**
  `CommandBuffer::pushReadback` (`ReadbackCommand` in `rendercommand.h`) reads a
  framebuffer's whole colour FIFO behind the commands before it. Like every
  non-draw command it bounds the GL draw sort, so it sees every draw pushed
  before it. The size is resolved on the render thread when the command runs
  (`GlBaseFramebuffer::getSize(Dispatched)`; a default framebuffer's pixel size
  lives only there) and returned in the `ReadbackImage`, so the caller never
  passes a size that a resize could have made stale. GL runs `glReadPixels`
  synchronously on the dispatcher thread
  (`GlRenderState::dispatchedRenderQueue`), which stalls the render thread until
  the preceding draws finish but never the loop thread, flips the rows to top
  first, and fulfils the command's promise there, so continuations run on the
  render thread. A failure is a `ReadbackFailure` carrying `Failed` or
  `Unsupported`. A submit dispatches at once, so a readback pushed outside a
  frame still completes. One still queued when the context is destroyed is
  dropped unrun (`Dispatcher::~Dispatcher`), and its future fails with
  `btl::future::BrokenPromise` on whichever thread drops it. The dummy queue
  fails it at once with `Unsupported`; `RenderContextImpl::canReadback` reports
  which a context does.
  A window's frame is read back by pushing one against `getDefaultFramebuffer()`
  on its main queue: queue order is the synchronisation, so it reads the last
  frame submitted, rendered this tick or not. Only an `OffscreenWindow` target
  is readable that way: an on-screen back buffer is undefined after the swap,
  so `GlBaseFramebuffer::canReadback` is false for the GLX/WGL window
  framebuffers and GL fails their readback with `Unsupported`. The real-GL
  check is the manual `bquiheadlesstest`.
- The root `meson.build` adds MSVC-style flags (`/wd4251`, `/bigobj`, `/UNICODE`)
  for any Windows build; those assume an MSVC-compatible driver, which is why
  clang must be `clang-cl`, not `clang++` (the build notes in the repo-root
  `AGENTS.md` cover this).
