# Design: `btl::RunLoop`, the platform run loop

> **Status: implemented (sockets, timers, post; POSIX + Win32).** This file pins
> the principles behind the design; the API contract itself is Doxygen in the
> `btl` headers. Verified against the `runloop-design` branch (2026-07-29). The
> parts still ahead of the code are the deferred backends and source kinds noted
> below.

## Purpose

One reactor that services window frames, sockets, timers, and file work from a
single loop. Today `bqui`'s `App::runUntil` has an `if (agentic)` branch that
runs a wholly separate `runSession` loop (its own reader thread plus a command
queue), so an app is *either* a normal windowed app *or* an agent-driven headless
one, never both. The run loop unifies those into one loop with pluggable sources.

Three payoffs:

- **A live remote channel on a real app.** A headful, free-running app can have a
  remote/inspection channel attached and serviced by the same loop - not only a
  headless, frame-stepped one.
- **Less concurrency.** The remote channel becomes one readable source on the app
  thread, which deletes the reader thread and command queue - the machinery whose
  abrupt-disconnect handling produced the SIGPIPE and SIGABRT bugs.
- **A general primitive.** A reactor is useful with zero graphics dependencies (a
  headless server, a CLI tool), which is why it lives in `btl`.

## Layering

- **`btl::RunLoop`** is the reactor: readable/writable/timer/post sources, and
  `run`/`stop`. It knows nothing about graphics, windows, or vsync.
- **`ase`** supplies the platform sources - window events and the vsync source
  (below) - and embeds a `btl::RunLoop` **by value** in `PlatformImpl`, exposing
  it via `runLoop()` so `bqui` and the remote layer register their own sources
  from outside.
- **`bqui`** builds a `FrameClock` pacing policy on top (below), and drives the
  loop from `App::run`.

## Core principle: the app never owns the iteration

The one rule that makes this portable to platforms whose loop is owned by the OS:
**the app registers sources and yields control; it never writes its own
`while (true) { pump(); }`, and it never relies on code after `run()`.**

- `run()` hands control to the loop and **may never return** (iOS, web).
- All app logic lives in source callbacks.
- Teardown is a callback the loop invokes on exit, not sequential code after
  `run()`.

The consequence for the backends: on a platform whose loop is native (iOS
`CFRunLoop`, Android `ALooper`, web), `RunLoopImpl` **must be a thin adapter over
that native loop, not a homegrown `select` loop** - otherwise the platform's
vsync source (which is delivered *by* the native loop) cannot reach it.

## Interface

`RunLoop` is a move-only value with an internal pimpl. Its public surface is
deliberately small - `run`, `post`, `stop` - because it is the *only* part that
is shared across threads. Everything else (registering sources and timers,
removing them) lives on a **`RunLoop::Controller`**, which the loop hands to every
callback for the length of that call. So the registration API is reachable only
from the loop thread: you are either already in a callback (you have a
`Controller`), or you `post` yourself onto the loop thread to get one.

Registering returns an RAII handle - `RunLoop::Source` / `RunLoop::Timer` - that
removes its registration when dropped. `detach()` opts out (a fire-and-forget
one-shot timer, or a source that lives as long as the loop).

Contract to pin, because it differs across backends:

- **Level-triggered** readiness: a readable/writable callback keeps firing while
  the handle stays ready (matches `select`), rather than edge-triggered `epoll`
  semantics.
- **Removal from within a callback is allowed** - a callback may drop a handle or
  `Controller::remove` a source by id.
- `run()` may not return; see the core principle.

## Thread safety and ownership

Only `post` and `stop` cross threads. The split is what makes that safe *by
construction*: the non-thread-safe API cannot even be named off the loop thread,
because a `Controller` only exists inside a loop-thread callback. A worker thread
holds at most a `RunLoop&` (for `post`/`stop`); to touch sources it posts a task
and is handed a `Controller` on the loop thread.

The payoff is that the loop's own state needs almost no locking:

- **Shared, so protected:** the posted-task queue (a mutex) and the running flag
  (an atomic). `post` appends under the lock and wakes the loop; `stop` clears the
  flag and wakes it.
- **Loop-thread-only, so lock-free:** the source and timer maps and the id
  counter. Nothing off the loop thread reaches them, so they carry no mutex.

Lifetime rules:

- `run()` pins a local `shared_ptr` to the impl before dispatching, so the loop
  survives its owning `RunLoop` being destroyed mid-run (for instance,
  reentrantly from a callback). When `run()` unwinds, that pin drops and the impl
  is freed on the loop thread.
- `~RunLoop()` asks the loop to stop but does **not** block - a join would
  deadlock the reentrant-destroy case, which is the only in-run destruction the
  contract allows. Destroy a `RunLoop` on its loop thread, or after `run()` has
  returned; do not destroy it from another thread while `run()` is blocked
  elsewhere.
- Calling `run()` again while it is already running throws.
- A handle's destructor may run on any thread: it removes its registration
  directly if it is on the loop thread, and otherwise posts the removal (a no-op
  if the loop is already gone).

## `NativeHandle`

The loop refers to an OS handle without exposing the platform type. `NativeHandle`
is an **opaque, fixed-size inline buffer** - non-owning, no RAII, trivially
copyable, and free of any platform header:

`NativeHandle` is a fixed-size inline byte buffer plus a small `Kind` tag,
default-invalid, with no platform header in the public type. The bytes are filled
and read only by two friend function templates (`makeNativeHandle` /
`loadNativeHandle`) that the per-platform conversion headers call - no general
access, no allocation.

- **The `Kind` tag says how to wait** (POSIX fd, Win32 `SOCKET`, Win32 `HANDLE`)
  so a backend picks the right wait primitive, but the payload type itself never
  escapes the platform conversion header.
- **Per-platform conversion headers, included only in platform code:**

  ```
  btl/posix/nativehandle_posix.h   NativeHandle fromFd(int);     int    toFd(NativeHandle);
  btl/win32/nativehandle_win32.h   NativeHandle fromSocket(..);  SOCKET toSocket(NativeHandle);
                                   // later fromHandle/toHandle - pulls in winsock/windows,
                                   // so it never escapes a platform .cpp
  ```

  A socket transport calls `fromSocket(sock)` in its own `.cpp` to register; the
  Windows `RunLoopImpl` calls `toSocket(h)` to wait on it. The winsock include is
  confined to those files; every other header just carries `NativeHandle` blind.
- **Storage bound** is generous: 16 bytes covers an fd (4), a `SOCKET`/`HANDLE`
  (8), or a pair (16). A platform that needs more is a one-line bump, not a
  redesign. The value need not be a scalar - a backend may store a small struct.
- **No ownership.** `NativeHandle` names a handle the transport still owns; the
  transport keeps it alive while registered and `remove()`s before `close()`. If
  an owning wrapper is ever wanted, it is a *separate* `Socket`/`OwnedFd` type
  that hands its non-owning `NativeHandle` to the loop.

This is the shape `libuv` (`uv_os_sock_t`), Asio (`native_handle_type`), and Qt
(`qintptr` handles) all use.

## Frames and vsync are not `btl` primitives

Vsync is a **platform source**, not a timer, because the model differs: iOS, and
Android, and the web *push* a frame callback at each display refresh with a
precise timestamp (`CADisplayLink`, `AChoreographer`, `requestAnimationFrame`),
auto-adapting to 60/90/120 Hz. A `btl` timer would drift, ignore variable refresh,
and fight the compositor there. So the vsync source lives in `ase`:

| platform | vsync source |
|---|---|
| iOS | `CADisplayLink` on the `CFRunLoop` |
| Android | `AChoreographer` on the `ALooper` |
| web | `requestAnimationFrame` |
| desktop | a timer, or a waitable swapchain (DXGI on Windows, GLX sync on Linux) |

It rides on the `RunLoop`; on the push platforms it is delivered by the same
native loop that `RunLoopImpl` adapts, alongside the sockets.

`bqui`'s **`FrameClock`** is the pacing policy on top - on-demand via
`signal::observe`, animating off the vsync source, with the GPU fence completing
*onto* the loop rather than blocking it. The full frame and window model - the
per-window armable vsync source, the on-demand cadence, and the window/surface
split for mobile - is its own design: [`frameclock.md`](frameclock.md). A `btl`
timer is only the desktop vsync fallback; frames are not part of the reactor.

## Relationship to the threadpool

`btl::RunLoop` (a single-thread main executor) and the existing `btl` threadpool
(a multi-thread worker executor) do not overlap. The seam between them is `post`:
blocking work runs on the pool and `post`s its completion back to the loop thread.
A common `Executor` concept, or loop-aware futures that complete *onto* a
`RunLoop`, is an attractive follow-up but not needed now.

## Source taxonomy and per-platform mapping

Most sources fit the readiness model - "the handle is ready, you do the
non-blocking op" - on every platform family:

| source | POSIX | Windows | iOS / Android | web |
|---|---|---|---|---|
| socket r/w, accept, connect | `select`/`epoll` fd | `WSAEventSelect` -> event, WFMO | `CFSocket` / `ALooper_addFd` | async cb |
| pipe / FIFO | fd | overlapped `ReadFile` -> event | same | - |
| FS watch | `inotify` / `kqueue` fd | `ReadDirectoryChangesW` -> event | `FSEvents` / `inotify` | - |
| timer | wait timeout / `timerfd` | WFMO timeout / waitable timer | `CFRunLoopTimer` / choreographer | `setTimeout` |
| cross-thread wake / `post` | `eventfd` / self-pipe | manual-reset event | `performBlock` / `ALooper_wake` | microtask |
| stdin / tty | fd | console HANDLE (WFMO) | fd | - |
| **regular file read/write** | **not selectable** | **overlapped / IOCP only** | - | - |
| vsync, window events | (ase) | (ase) | native | native |

Windows bridges its overlapped (completion) handles for pipes and FS-watch into
readiness by signalling an event, which WFMO waits on.

## The readiness-vs-completion fault line, and files

POSIX is natively a **readiness** model (`select`/`epoll`); Windows is natively a
**completion** model (IOCP: "your read finished, here is the data"). For sockets
and pipes the two reconcile (Windows fakes readiness with `WSAEventSelect` and
overlapped-to-event). **Regular file read/write fits neither readiness model**: a
POSIX file fd is always reported "ready" yet the read still blocks on disk, and
Windows file I/O is completion-only.

Decision: **file I/O is not a reactor source.** It is threadpool-offload plus
`post` - a blocking read/write on a worker thread that posts its result to the
loop. That is thread-offloaded blocking I/O, which is all a GUI toolkit needs
(assets, config, hot-reload). True async file I/O (Windows IOCP, Linux
`io_uring`) is a deferred optimization behind the same surface, not a gap.

## Known limits and non-goals

- **Windows `WaitForMultipleObjects` caps at 64 handles.** Fine for a GUI and for
  #96 (one socket); a high-connection-count server would need IOCP, which slots
  behind the same `addReadable`/`addWritable` API later.
- **True async file I/O** (IOCP / `io_uring`) is deferred.
- **Signals (`signalfd`), child-process exit (`pidfd` / process HANDLE), and
  device/serial I/O** are out of scope - server and tooling concerns a UI toolkit
  rarely needs. Each is just another handle kind on the same reactor if wanted.

## Minimal scope for #96

Implement `btl::RunLoop` with a **POSIX (`select`)** and a **Windows
(`WSAEventSelect` / WFMO)** backend, **sockets only** - `addReadable`, `addTimer`,
`post`, `run`/`stop`, with `addWritable` present in the interface. That covers the
Linux, macOS, and Windows CI legs.

#96 then reworks `runUntil` / `runSession` onto it: one loop, the remote socket is
one `addReadable` source (reader thread and command queue gone), and run/pause/step
become `FrameClock` timer and `post` operations.

Win32 `HANDLE` sources (for overlapped-I/O events, e.g. named pipes) are also
implemented. Deferred behind the same interface, needing no rework to add later:
the iOS, Android, and web `RunLoopImpl`s; FS-watch and file I/O sources; and the
`addWritable` backend.

## Sequencing

1. This doc.
2. Implement the minimal `btl::RunLoop` - its own PR, lands **before** #96.
3. Rework #96 onto it.
4. Rebase #122 and #123.
