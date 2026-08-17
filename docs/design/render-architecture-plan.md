# Execution plan: rework #140 to the render architecture

> **LANDED.** R1-R5 are all implemented and CI-green on `session-refactor` (#140,
> 2026-08-17). Kept as the record of the staging that got there; the design in
> [`render-architecture.md`](render-architecture.md) is the source of truth for the
> built model.

> **Status: active execution spec.** Companion to
> [`render-architecture.md`](render-architecture.md) (the design; source of truth for
> *why*). This is the staging *how* for reworking the `session-refactor` branch (#140)
> **from the Session model to the new architecture** — windows carry their context, the
> loop is context-free, `Session` is dropped, the platform owns a default loop, remote
> is a two-mode driver. Supersedes `render-session-plan.md` (the Session-based plan,
> kept only for its record of what Stages 1-4 landed = the carry-forward below).
> Anchors are file/function level — verify exact locations against current HEAD.

## Carry-forward from #140 (do NOT redo)
Stages 1-4 already landed these and the rework builds on them:
- `ase::PresentStatus` + present as a **status-returning surface virtual**
  (`presentstatus.h`; `GlxWindow::present`/`WglWindow::present`/`OffscreenWindow::present`/
  `DummyWindow::present`), dispatched via the **GL-free `RenderQueue::present`** seam
  (`renderqueue.h`/`renderqueueimpl.h`/`glrenderqueue.cpp`/`dummyrenderqueue.cpp`),
  triggered from `avg::Painter::presentWindow`. The `getImplOfType` present chain is gone.
- Unified **`makeWindow(RenderContext&, size, headless)`** across Platform/PlatformImpl/
  backends/`bqui::WindowImpl`.
- `needsRedraw()`/`frame()` off the public `ase::WindowImpl` (currently `friend class
  Session` — R3 re-homes the friendship).

## Global invariants (every stage)
1. **CI green is necessary but NOT sufficient.** The dummy backend never draws/presents,
   so App/loop control flow, remote, and the submit/link seam are CI-covered on all legs,
   but **GLX/WGL real swap + real offscreen are CI-dark**. Every stage touching the loop,
   window, present, or context adds a manual gate: **clang-cl build + `bquiheadlesstest`**
   (and a quick `testapp1`) on a real-GL machine, and (for GLX-only changes) push for the
   Linux CI compile legs.
2. **Present only surfaces that drew this tick** (WGL `SwapBuffers` flushes the current
   context — presenting a non-drawn window is a bug).
3. **Recreation-tolerance shape** (design-level; GL always returns `Ok`): `present()` and
   the new `acquire()` return `PresentStatus`; the loop/window is written to *tolerate a
   status that demands recreation* and "no valid surface," even though no code acts on it
   until a non-GL backend lands. Do not bake in device/context immortality.
4. **Watch the parked MSVC crash.** R2/R3 rework the exact GL context/dispatcher startup
   where the parked intermittent crash lived (unguarded render-worker exception; suspect
   unchecked `wglMakeCurrent` on the shared dummy HDC). If it recurs on a changed base,
   capture the stacktrace (do NOT add a try/catch — the uncaught throw is intentional
   fail-fast; see the memory). Otherwise note it did not recur.

## R1 — RunLoop default + injection
**Goal:** `RunLoop::makeDefault()`/`getDefault()`; the loop is created by the dev/App and
injected into the platform, not owned by the platform.

**Touches:** `btl` `RunLoop` (`src/btl/include/btl/runloop.h` + `runloop.cpp`) — add the
static `makeDefault()`/`getDefault()`, an `std::atomic<RunLoop*>` global, RAII default
registration (register in ctor / compare-exchange-clear in dtor), throw-on-second-default,
make `RunLoop` non-movable. `PlatformImpl` currently owns `btl::RunLoop loop_` — change the
platform ctor to **take a `RunLoop&`**; `makeDefaultPlatform`/`makeDummyPlatform`
(`src/ase`) take the loop; `App::runUntil` (`app.cpp`) creates `RunLoop::makeDefault()`
(named local, outlives the platform) and passes it in.

**DoD:** `RunLoop` has `makeDefault`/`getDefault` (atomic, RAII, throw-on-double,
non-movable); the platform holds an injected `RunLoop&`; App creates+injects it;
`getDefault()` reachable for IO.

**Green gate:** a `btl` unit test for `makeDefault`/`getDefault` (registration, throw on
second, RAII clear on scope exit) — this is genuinely unit-testable. apptest + build.

**Traps:** `RunLoop` non-movable + address-stable; the loop must outlive the platform
(named local ordering); the platform no longer owns its loop.

## R2 — Window carries its context/queue; present + `framesInFlight` move onto the window
**Goal:** the *(surface, context/queue)* binding, present, and backpressure all live on the
window; `Session` no longer holds a context or a fence.

**Touches:** `ase` `WindowImpl` base + `GlxWindow`/`WglWindow`/`OffscreenWindow`/
`DummyWindow` — **store the `RenderContext`/queue** the window is created against (already
passed to `makeWindow`; now retained). `present()` uses the stored queue/context (GL:
dispatch the swap onto its context's render thread, as today, but reached via the stored
context, not a threaded-in one). Add **`acquire()`/`beginFrame()`** returning
`PresentStatus`: GL implementation is "wait until this window's in-flight count is under
budget" (the per-window `framesInFlight` + its fence). Move the fence/`framesInFlight`
bookkeeping **out of `Session::run` (`session.cpp`) onto the window** — each window submits
its own fence on its own queue and decrements its own count on completion. `bqui::WindowImpl`
stops holding a bare `RenderContext&` and defers to the ase window's stored context.

**DoD:** window stores its context/queue; `present()` uses it; per-window
`acquire()`+`framesInFlight` (status-returning); `Session::run` no longer submits a fence
or reads a context.

**Green gate:** dummy path (no-op present/acquire, immediate completion) on CI; **manual
real-GL for the fence/backpressure/swap** (`bquiheadlesstest` several frames + `testapp1`).

**Traps:** CI-dark; the fence-completion-decrements-count must move from `Session`'s
`runLoop().post(--framesInFlight)` to per-window completion; keep invariant 2; this is
where the parked crash may recur (invariant 4).

## R3 — Drop `Session`; the platform owns a context-free loop + `run`/`pause`/`step`
**Goal:** delete `Session`; the platform drives a context-free tick over windows and
exposes manual driving.

**Touches:** **delete** `src/ase/include/ase/session.h` + `src/ase/src/session.cpp`;
remove `makeSession` from `Platform`/`PlatformImpl`/backends/remote. Add
**`Platform::run(frameCallback)`** (context-free tick: per dirty window, `acquire` →
`frame`/render → `present`, all via the window's own context) + **`pause()`** (returns a
RAII token) + **`step(dt)`** (on the token). The tick body (cadence, re-arm, wake sources,
`scheduleTick_`, backpressure-now-per-window) moves from `session.cpp` onto the platform's
`run` (GLX/WGL share it; dummy's degenerate form + `maxFrames`→step-budget folds in as
before). `App::runUntil` (`app.cpp`): replace `platform.makeSession(context).run(cb)` with
`platform.run(frameCallback)`; App still makes the `RenderContext` and creates windows with
it (`makeWindow(context,…)`), but does not thread it into `run`. Re-home the
`needsRedraw`/`frame` friendship from `Session` to whatever drives the tick.

**DoD:** no `Session`/`makeSession`; `platform.run(frameCallback)` + `pause()`/`step(dt)`
(RAII token); context-free tick; App drives it; behaviour matches the current loop.

**Green gate:** apptest + sessiontest on dummy (the tick + pause/step + re-arm) — strong
automated coverage of the control flow; **manual real-GL** for the actual draw/present loop.

**Traps:** the biggest structural change; CI-dark for the GL loop; the pause-token RAII
semantics (paused suspends frame production, not the reactor); preserve on-demand re-arm /
wake-on-event / wake-on-`requestFrame`. Invariant 4 (crash watch) applies hardest here.

## R4 — Remote as the two-mode driver
**Goal:** replace the remote *platform decorator* + `runSession` with a thin bqui
`RemoteDriver` over `pause`/`step` + the default loop.

**Touches:** **delete** the remote platform decorator (`src/bqui/src/remote/remoteplatform.{h,cpp}`
— `RemotePlatformImpl`, `RemoteWindowImpl`) and the `RemoteApp`/`runSession` driving.
**Reuse** the transport + JSON-RPC protocol (`remote/transport.*`, `remote/session.{h,cpp}`
handlers) but rewire them into a new **`RemoteDriver`** that: registers the client socket
on `RunLoop::getDefault()`, optionally holds a `pause()` token, and maps `advance→step(dt)`,
`inject→window.inject`, `introspect/renderTree→` the **glue's** snapshot (bqui `WindowImpl`
already exposes `getResolvedIntrospection()`/the render-tree — no `RemoteWindowImpl`).
`App::runUntil`: drop the local-vs-remote fork; if an endpoint is set, attach the
`RemoteDriver` before `platform.run(frameCallback)`. Two modes = driver with/without the
pause token.

**DoD:** no `RemotePlatformImpl`/`RemoteWindowImpl`/`runSession`; `RemoteDriver` over
`pause`/`step`; introspection via the glue; both modes; `App::runUntil` has one frame path.

**Green gate:** **sessiontest, reworked to the new driver — the strongest automated signal
of the plan** (it steps a real dummy App through the driver). Re-run the live MCP inject/
introspect check.

**Traps:** significant rewire of the #139 inspector layer, but transport/protocol/
introspection carry over — only the driving changes. Preserve the wire schema
(`kind`/`pointer`/`button`, the inject validation) so existing clients/tests keep working.

## R5 — Ownership + recreation reservations
**Goal:** upward strong-ref RAII ownership; delete the interim lifetime dance.

**Touches:** make the refs upward-strong via the existing value handles — `bqui` glue holds
its ase `Window` strongly; the ase `Window` stores its `RenderContext` (value handle =
`shared_ptr<RenderContextImpl>` = strong); `RenderContext` holds its `Platform` (value
handle = strong). `Platform` keeps a **weak_ptr** to windows for event routing only.
Delete `App::runUntil`'s `ImplScope`/`PlatformScope`/`runningPlatform_` dance (`app.cpp`) —
RAII teardown replaces it — while preserving the one real ordering constraint (the queue
must finish before a window's framebuffer is released; now expressed by dtor order:
window drops → its queue finishes → framebuffer released). Land the recreation-tolerance
shape (invariant 3): `present()`/`acquire()` status handling, "tolerate no valid surface."

**DoD:** upward strong-ref ownership; lifetime dance gone; teardown RAII-ordered and
correct; recreation reservations shaped (no-op on GL).

**Green gate:** apptest `AppWindows` + sessiontest `dynamicWindowsOpenAndCloseById`
(mount/unmount + teardown correctness) on dummy; **manual real-GL** for real
framebuffer/queue teardown ordering.

**Traps:** teardown ordering is the classic hazard — verify no use-after-free of a
framebuffer while its queue has work in flight; the value-handle strong chain must not
create a cycle (Platform → weak to windows breaks it).

## Order and notes
- Order: **R1 → R2 → R3 → R4 → R5.** R2 must precede R3 (context-free loop needs
  window-carries-context). R4 needs R3 (pause/step). R5 is polish over the rest.
- Riskiest: **R2 and R3** (structural + CI-dark). Land each in reviewable sub-commits; do
  not skip the manual real-GL gate.
- Each stage is a commit set on `session-refactor`; accumulate, squash/rearrange before
  merge (do not reshape via amend/reset mid-flight).
- Before requesting human review of the whole reworked stack: clean-context style +
  correctness reviews, docs-freshness (this file + `render-architecture.md`), and a clean
  manual real-GL pass (ideally one MSVC).
