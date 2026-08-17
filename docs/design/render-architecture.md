# Design: windows carry their context, the loop does not

> **Status: design settled through discussion, not built.** Supersedes the
> `Session` model in [`render-session.md`](render-session.md) — that note extracted
> a `Session` binding loop + context + windows; this one records why all three
> bindings dissolve and what replaces them. Design-only; no implementation implied.
> Reflects reasoning as of the render-session refactor branch (`session-refactor`,
> 2026-08).
>
> **Feasibility-checked** against GLX, WGL, Vulkan, D3D12, Metal (macOS + iOS), and
> Android (Vulkan + GLES): feasible on all eight, with the present contract, the
> OS-loop wrapper handling for the default loop, the cadence rationale, and the
> ownership recreation-reservations amended below (folded in). Manual `pause`/`step` and the
> context-free loop hold unchanged everywhere; the only real risk is device/surface
> *recreation*, addressed under *Ownership*.

## Why this supersedes the Session model

`render-session.md` introduced a `Session` to bind a run loop, one `RenderContext`,
and its windows. Working the design further, every one of those bindings came apart:

- the **context** moved onto the **window** (a window stores the context/queue it
  was created against),
- the **windows** are owned by **App** (via its glues), and the loop merely
  iterates them,
- the **loop** became **context-free** — it drives windows that each carry their
  own context, so it never names one.

With nothing context-specific left to bind, `Session` is a solution to a problem we
no longer have, and is **dropped**. What remains is a loop (a general reactor) and
frame-driving (cadence + backpressure) as an operation over windows — both of which
land naturally on the `Platform`.

## The pivot: the binding lives on the window

The load-bearing move. A **window carries its `RenderContext`/main queue** (bound at
creation, non-transferable) plus its swapchain-or-FBO. Two consequences cascade:

- **Present is the window's own operation** — the window *sequences its own present
  relative to its own submit* (see *Present*). Not a command in a stream, not
  something a session threads a context into.
- **The loop needs no context** — it drives windows; each self-renders and
  self-presents through the context it carries. This is what finally makes
  multi-context real: different windows can be on different contexts and the loop
  does not care.

This is grounded in how the modern APIs bind, all creation-time and non-transferable:
- **D3D12** — the swapchain is created *for a command queue* (`CreateSwapChainForHwnd(queue, hwnd)`); present flips through that queue.
- **Vulkan** — the swapchain is created on the device from the window's surface;
  present is `vkQueuePresentKHR(queue, {swapchain, imageIndex, waitSemaphores})`. The
  spec only requires the queue be *present-capable* for the surface — a compatibility
  constraint, not an ownership — so **a window storing the queue it was made against
  makes `present()` valid by construction.**

## The layers

- **Platform** — OS backend + factory: `makeRenderContext()`, `makeWindow(RenderContext&, size, headless)`, OS event pumping. Receives a `RunLoop` at construction. Owns the frame-driving API (`run(frameCallback)`, `pause()`, `step(dt)`) over that loop. Holds a **weak_ptr** to each window for event routing. No `run`-state beyond the loop; no `Session`.
- **RunLoop** (`btl`) — the reactor: readable/writable/timer/post. OS-owned (iOS `CADisplayLink`, Android `Choreographer`) or self-driven. Created by the dev and injected into the platform. See *The default loop*.
- **RenderContext** — the device + its main queue (fence + FIFO). Made by the platform.
- **Window** — carries its `RenderContext`/queue + swapchain-or-FBO; owns its
  `framesInFlight` and the `acquire`/`present` for its surface. Interface to bqui is
  just **`setFrameCallback(cb)`** (ase calls it when it is time to produce a frame)
  and **`requestFrame()`** (bqui calls it when state changed). One `makeWindow`
  unifies on-screen (swapchain) and offscreen (FBO).
- **App** (bqui) — owns the **glues** that bridge an ase window to the widget /
  render-tree / painter (the current `WindowImpl` role). Parses env, picks the
  platform, sets frame callbacks, calls `requestFrame`, drives `run` (plus
  `pause`/`step` for the manual/remote case). No local/remote/offscreen branches in
  the frame path.

## Present

On the window (above). The `getImplOfType` present chain and present-as-command are
gone (already true after the refactor's Stage 1); this note completes it by making the
*(surface, queue)* binding explicit on the window rather than resolved at present time.

**Contract (validated against the backends):** state it at the *sequencing* level, not
the queue-call level — **"the surface sequences its own present relative to its own
submit,"** and `present()` **owns or accepts the sync primitive** it needs. Only D3D12
matches the literal "flip through the stored queue" phrasing (`Present1` on the queue
the swapchain was created for). The others *sequence rather than call*: Metal records
`presentDrawable` on the frame's command buffer (presented after it completes); Vulkan
presents with the render-finished semaphore the draw submit signals; GL dispatches the
swap onto the thread its context is current on. Stating present at the sequencing level
covers all four without a per-backend reshape (this is the wording `render-session.md`
already used; keep it).

## Backpressure is per-window `acquire`

Backpressure is a **surface** property, not a shared-context one — which is exactly
how the modern APIs already work, because `acquire` is per-swapchain and a swapchain
is per-window:

| Backend | Backpressure = the window's `acquire` |
|---|---|
| WGL/GLX (threaded) | the fence — `framesInFlight` on that window's context queue |
| Metal / iOS | `nextDrawable` blocks past `maximumDrawableCount` |
| Vulkan / Android | `vkAcquireNextImageKHR` on the swapchain's image pool |
| GLES / Android | `eglSwapBuffers` blocks against swap-chain depth |

So `framesInFlight` is a **window property**. The window owns the in-flight *count*
and the *"am I ready?"* gate; the context/queue owns the *fence primitive* and FIFO.
The context-free tick becomes, per dirty window: `acquire` (throttles = backpressure)
→ render → `present`.

Honest note on the shared GL queue: per-window counters give per-window *bounds*, but
a shared FIFO couples their *timing* (a fence waits for everything ahead of it, so a
slow window drags the others). That is not a misplacement — it is the physical truth
that windows on one context share one GPU pipe. Genuine independent pacing is what
separate contexts buy, and then the same per-window counter is independent for free.

## Cadence

**One loop, one master cadence, harmonic per-window subdivision (120/60/30…).** This is
a **portability/simplicity choice, not an OS prohibition** (the audit corrected the
earlier, too-strong rationale): iOS/Android *do* allow independent render threads, but
they require the vsync *timing* to originate from the OS (`CADisplayLink` /
`Choreographer` — the latter explicitly "a recommendation"). Rather than fork per
platform, we take one loop with harmonic subdivision everywhere and make
fully-separated cadence a **non-goal**. A window renders every Nth tick; the loop stays
single-threaded and portable.

Single loop is not single-threaded *submit*: the loop thread drives cadence + events,
but per-context command building/submission fans out to a **threadpool** (the async
graphics abstraction, kept as-is — submit from any thread, complete asynchronously;
GL hides a serializing lane, Vulkan uses the pool properly). So multi-context gets
real parallelism on submit while sharing one cadence loop. Per-window backpressure
means a tick never blocks on any one context.

**Multi-context needs one loop; multiple loops is a separate, desktop-only non-goal.**

## Manual frame driving — and it is not remote-specific

A universal control on the platform's driver, orthogonal to cadence:

- **`pause()` returns a RAII token**; while held, auto-cadence frames are suspended
  (the loop *keeps pumping events/IO* — "paused" suspends frame *production*, not the
  reactor).
- **`step(dt)`** lives on that token — so you *structurally cannot* step a
  free-running loop, and there is no runtime no-op to reason about. `dt` is
  caller-supplied (deterministic time).
- Dropping the token resumes free-running. The token's lifetime *is* the mode:
  "manual while a driver is attached, auto otherwise" — no mode flag.

This subsumes three ad-hoc mechanisms into one: the dummy backend's `maxFrames`
budget, the test harnesses bounding frames, and remote's `advance(dt)`. Because
remote uses the same primitive tests and dummy use, it stops being special.

`step` is **app/run-level** (one app frame: advance signal time by `dt` → reconcile →
render dirty windows) — the *same frameCallback path* as an auto tick, so bqui has no
special frame code. Because `run()` blocks, the pause token and any I/O sources are
set up *before* `run()`, and `step` fires from *within* the loop via those sources
(single thread).

## The default loop

The loop is needed for IO (socket waits) independent of rendering, so it must be
reachable without going through bqui's App. Default-ness lives on the **loop**, not
the platform:

- **`RunLoop::makeDefault()`** constructs a loop registered as the process default;
  **`RunLoop::getDefault()`** returns it. `RunLoop` is otherwise an ordinary,
  freely-constructible class — `makeDefault`/`getDefault` are the opt-in default
  facility (many loops may exist; zero-or-one is "the default").
- **OS-loop-owning platforms need no separate API.** `RunLoop` is an interface over a
  platform-specific `LoopImpl`; on iOS/Android that impl is not a real loop but a thin
  **wrapper over the OS loop**. `makeDefault()` still just builds the `RunLoop` handle
  (nothing new is *constructed* — the impl references the existing OS loop), and `run()`
  there hands control to the OS entry point (iOS `UIApplicationMain` / the main
  `CFRunLoop`), which never returns — exactly matching "run() blocks" everywhere else.
  The wrapper translates `addReadable`/`addTimer`/`post` into the OS loop's primitives
  (`CFRunLoop` sources, `ALooper` fds) and sources the cadence from OS vsync
  (`CADisplayLink` / `Choreographer`). The OS-owned-vs-self-driven distinction lives
  entirely inside the impl; the interface, injection, and RAII/atomic default
  registration are identical on every platform.
- Registration is **RAII on the loop**: it registers in its ctor and deregisters in
  its dtor, so the default can never outlive the loop (no dangling). `makeDefault`
  **throws if a default already exists** (catches accidental double-init).
- The global is an `std::atomic<RunLoop*>` (get is an atomic load). The only residual
  race — `getDefault()` during teardown — does not occur for a loop created at
  startup and destroyed at shutdown; a `shared_ptr` default would close it formally
  but is not worth the ownership cost.
- The loop is **injected into the platform** ctor (`makeDefaultPlatform(loop)`),
  which makes it explicit, testable, and shareable.

```cpp
auto loop = btl::RunLoop::makeDefault();   // registers as the default
auto platform = makeDefaultPlatform(loop); // platform receives it
// IO anywhere: btl::RunLoop::getDefault()
```

Two safety mechanics: **`RunLoop` is non-movable** (the default is a pointer to it,
so it must be address-stable; guaranteed elision keeps `auto loop = makeDefault();`
working), and **the loop must be a named local that outlives the platform** — passing
`makeDefault()` inline as a temporary (`makeDefaultPlatform(RunLoop::makeDefault())`)
dangles. `getDefault()` returns a reference and throws when absent (a precondition for
its IO callers); a `tryGetDefault()` returns a pointer for the check-first case.

## Ownership and teardown

Refs point **upward and strong**, weak only for events, so destruction order is
structural rather than hand-policed:

- glue (App-owned) → **strong** → `Window` → **strong** → `RenderContext` → **strong**
  → `Platform`.
- `Platform` → **weak_ptr** → windows (event routing only).

"The platform can't die before its windows" is then automatic: a window keeps its
context alive, a context keeps the platform alive, so the platform outlives every
window by construction. App drops glues → windows → (last) context → (last) platform,
in order, for free. The interim's `ImplScope` / `finish()`-before-teardown /
`runningPlatform_` dance all dissolve.

**But the device/context is not immortal — the recreation reservations must come back.**
This is the audit's one genuine over-promise (and a regression this note introduced over
`render-session.md`). Upward-strong-refs are correct for steady-state destruction
*ordering*, but **no non-GL backend guarantees a permanent device/surface**: D3D12
`DEVICE_REMOVED` and Vulkan `DEVICE_LOST` tear down the whole device (queues, swapchains,
the "context" the window strong-refs), and Android destroys the surface on backgrounding
and rotation. So:

- the window must be able to **rebuild its swapchain/surface — and, rarely, its device —
  at runtime, beneath a surviving owner**;
- the "outlives" invariant is anchored on the **platform/factory**, not on an
  assumed-permanent per-window device;
- and the three reservations `render-session.md` had and this note dropped come back:
  **`present()`/`acquire()` return a status that can demand recreation**, and the
  **window/App must tolerate having no valid surface** (mobile background, rotation,
  device-lost).

GL hides all of this because its context is effectively permanent for the window's life;
the model must not bake that assumption in. Upward-strong-refs stay — they just order
destruction, they don't promise immortality.

## Remote

Remote is **not** a branch in the frame path and **not** a special ase platform. It is
a thin bqui driver over the universal primitives — `pause`/`step`, the default loop,
`window.inject`, and glue introspection — so the frame path is identical to local.
Three layers, all in bqui:

- **Transport** — the client socket + framing (already built in the inspector work),
  registered as a source on the default loop
  (`RunLoop::getDefault().addReadable(sock, onMessage)`). The app connects out to the
  configured endpoint. This is the default-loop-for-IO case realized.
- **Protocol** — the JSON-RPC dispatch: `advance` / `introspect` / `renderTree` /
  `inject` / `describe`.
- **Driver** — the glue to ase: optionally hold a `pause()` token, and map each protocol
  message to a universal primitive.

Message → primitive:
- `advance(dt)` → `token.step(dt)` — one client-clocked frame, the *same* `frameCallback`
  path as an auto tick.
- `inject(windowId, event)` → `window.inject(event)` — the ase window's inject, feeding
  the normal widget event path; the effect appears on the next frame.
- `introspect` / `renderTree(windowId)` → read the **glue's** widget/render-tree snapshot
  and reply.

**Two modes, same transport and protocol — the only difference is whether the driver
holds a pause token:**
- **Client-driven (deterministic, headless):** the driver pauses; `advance(dt)` steps,
  the client owns the clock. Deterministic (inject → step → observe). For automated
  testing and headless inspection.
- **Observer / live-attach:** no token; the app free-runs on its own cadence and the
  client `introspect`s/`inject`s *between* auto frames. Live but not deterministic (an
  inject lands whenever the next auto frame renders). For attaching to a running app —
  the earlier inspector could only do the first.

**App wiring:** if a remote endpoint is configured, App attaches a `RemoteDriver` (given
the loop, the platform, and its glue registry) *before* `platform.run(frameCallback)`.
The driver registers the socket and, in client-driven mode, pauses. `run()`,
`frameCallback`, window creation, present — identical to local; the only branch is
"attach the driver or not," a thin composition rather than a reimplemented loop.

**`RemoteWindowImpl` dissolves.** Introspection is a capability of the **glue** (it holds
the window + widget + render tree), so it is available for *any* window — no
remote-specific window wrapper, a whole layer the interim carried that this model drops.

**Threading:** the socket source and the frame tick both run on the loop thread, so
`advance`/`introspect`/`inject`/`step` serialize with frames — no cross-thread
coordination (unlike the interim's `runSession`). Introspection reads CPU-side
app/render-tree state the loop thread owns; the async graphics threadpool only holds
already-built command buffers, so there is no race.

**Why this is lighter than the interim:** the interim forks `App::runUntil` into building
a `RemoteApp` + a parallel `runSession` loop. Here there is no parallel loop — remote
pauses the *one* loop and steps it from a socket source, and the genuinely-bqui parts
(transport, protocol) are reused unchanged.

**Open details (to pin at implementation; none blocking):**
- `step(dt)` must surface the keep-running result so the driver can stop the loop when
  the app wants to exit.
- Initial `App::sync` (window mount) runs at `run()` start regardless of pause, so an
  `introspect` before the first `step` sees mounted windows.
- Disconnect lifecycle: drop the token (resume/observe) or stop the loop (exit) — an
  inspector likely ends the session.
- Pixel readback (vs logical trees) is deferred — it is offscreen-`present`-as-readback,
  a future add, not a gap in this model.
- One client is the assumed boundary (two `step`ing clients would be two clocks).

Feasibility: remote fits the model with no unresolvable issue found; the substantive
addition over the first sketch is the **two-mode driver** (optional pause token), which
the context-free-loop + `pause`/`step` design supports for free.

## What this resolves (vs the interim refactor)

- `App::runUntil`'s incidental weight — the lifetime dance and the local-vs-remote
  fork — dissolves (RAII ownership; one frame path).
- `Session::run`'s hand-rolled `framesInFlight`/fence-post/re-arm moves onto the
  window (`acquire`) and the loop's cadence.
- Multi-context becomes real (windows on different contexts, one loop, parallel
  submit) instead of the false generality `run(RenderContext&)` implied.
- Offscreen and on-screen unify (one `makeWindow`, present = swap or readback).
- The graphics async abstraction is untouched — only present, the binding, and the
  loop/ownership around it change.

## Non-goals

- Reworking the async graphics abstraction (submit-from-any-thread, async
  completion) — it stays; only present/binding/loop change.
- Multiple independent loops / fully-separated cadence (breaks the OS-owned single
  loop on mobile).
- A hard global platform singleton (fights platform-swapping for tests/remote); the
  default *loop* carries the ergonomics instead.
