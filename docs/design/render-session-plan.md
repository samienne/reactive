# Execution plan: the render-session refactor

> **Status: active execution spec.** Companion to
> [`render-session.md`](render-session.md) (the design; that is the source of
> truth for *why*). This file is the staging *how* — ordered stages, each an
> individually CI-green commit set. Transient: squash/rearrange before merge, may
> be removed from the final PR. Base: stacked on `agentic-rebuild` (#139),
> unmerged; the whole system (inspector + session) is evaluated together before
> anything hits master.

## Global invariants (apply to every stage)

1. **CI green is necessary but NOT sufficient for render-loop work.** apptest +
   sessiontest run on every CI leg via the dummy backend, but the dummy **never
   draws or presents** (`DummyPlatform::run` only calls `frameCallback`;
   `DummyRenderQueue::submit` discards). So CI covers App/session control flow, the
   remote/`advance` path, and the submit/link seam — but the **GLX/WGL real swap
   and real-offscreen rendering are CI-dark**. Any stage touching the loop, present,
   or offscreen render **must** add a manual gate to its definition-of-done:
   local **clang-cl build** + run **`bquiheadlesstest`** (the unregistered
   `headlessrendertest` target) on a machine with a real GL context.
2. **Present only surfaces that actually drew this tick.** The WGL swap
   (`SwapBuffers(hdc)`) flushes the *currently current* context; presenting a
   window that drew nothing this frame swaps another window's context. The current
   `onFrame` path satisfies this for free; the Session must preserve it.
3. **Forward-compat surface shape (validated vs Vulkan/D3D12/Metal/Android).**
   Two guards, both no-ops on GL, so a non-GL backend lands without reshaping:
   - **`Surface::present()` returns a status, not `void`** (GL always returns
     `Ok`). A `void` return is the costliest thing to retrofit — every non-GL
     backend can fail present and demand swapchain recreation. Land this in Stage 1.
   - **The Session's draw step asks the surface for this frame's render target**
     rather than assuming a fixed framebuffer, so a `Surface::beginFrame()` /
     `acquire()` hook (acquire -> render -> present) slots in later. GL no-op
     returns the default framebuffer. Shape this in Stage 2/4; do not foreclose it.
   - Do not bake "session presents strictly after submit" into the contract wording
     (Metal records present into the command buffer before commit); present is "the
     surface sequences present relative to its own submit."

## Stage 1 — Present becomes a surface operation

**Goal:** delete present-as-command; make `present()` a status-returning virtual on
the window/surface, dispatched by the caller after the draw submit.

**Touches (verified file:line on this base):**
- `src/ase/include/ase/rendercommand.h:24,52` — remove `PresentCommand` struct +
  variant member.
- `src/ase/include/ase/commandbuffer.h:38`, `src/ase/src/commandbuffer.cpp:47-49`
  — remove `pushPresent`.
- `src/ase/src/gl/glrenderstate.cpp:283-292` — remove the `PresentCommand` branch.
  **TRAP: that branch also calls `endFrame()` (`glrenderstate.cpp:290`, def
  :232-237), which resets `boundUniformSet_/boundVbo_/boundIbo_`.** Relocate that
  per-frame reset (end of `dispatchedRenderQueue`) or it is silently dropped.
- Delete the present chain + callback plumbing: `GlRenderContext::present`
  (`glrendercontext.cpp:71-79`), `GlxRenderContext::present`
  (`glxrendercontext.cpp:40-47`), `WglRenderContext::present`
  (`wglrendercontext.cpp:38-53`), `presentCallback_` (`glrenderqueue.cpp:12`,
  `glrenderstate.cpp:107,110,47`, `glrendercontext.cpp:44-47`).
- Present virtual on the window: `GlxWindow::present(Dispatched)` already does the
  real work (`glxwindow.cpp:461-478`, keeps its X lock/XSync **inside**).
  **`WglWindow::present()` is currently an empty stub (`wglwindow.cpp:439-441`)** —
  move the `SwapBuffers(hdc_)` into it (it holds `hdc_`); unify the signature with
  GlxWindow's. `OffscreenWindow::present()` = the no-op the base currently
  special-cases.
- **New public, GL-free seam** to enqueue present behind submitted draws: today
  `dispatch` is GL-typed and impl-only (`glrenderqueue.h:20`, takes
  `std::function<void(GlFunctions const&)>`) — do NOT leak `GlFunctions` into a
  public header. Add e.g. `RenderQueue::present(Window&)` forwarding to the impl's
  dispatcher; `DummyRenderQueue` implements it too (calls `window.present()` no-op)
  so the dummy/remote path links.
- Caller: `avg::Painter::presentWindow` (`painter.cpp:165-170`) — flush the
  accumulated draw buffer, then `queue.present(target)`. **Ordering:** `onFrame`
  calls `presentWindow` before `flush()` today (`windowimpl.cpp:381-384`); present
  must be dispatched **after** the draw buffer is submitted. FIFO on the single
  dispatcher keeps draws->present->fence order identical to today.
- **`present()` returns a status enum** (`Ok`/... — minimal now, GL always `Ok`).
  See invariant 3.

**DoD:** `PresentCommand` and the `getImplOfType` present chain gone; `WglWindow`
does its own swap; GL-free `RenderQueue::present` seam exists + Dummy implements it;
`endFrame()` reset relocated; present dispatched only for drawn windows;
`present()` is status-returning.

**Green gate:** sessiontest/apptest exercise the submit+dispatch seam on dummy (CI,
all legs). **Manual: clang-cl build + `bquiheadlesstest` for the real swap.**

**Independent of Session** — lands first so Stage 2 builds on surface-virtual
present rather than reworking a command.

## Stage 2 — Introduce `Session`; route all backends through one loop body

> **Landed on `session-refactor`.** `ase::Session` (`src/ase/include/ase/session.h`,
> `src/ase/src/session.cpp`) owns the tick; GLX, WGL and dummy build a
> `Session::Config` and hand off to `Session::run`. Notes vs the sketch below:
> - **Anchors were stale.** The GLX/WGL sources are under `src/ase/src/glx/` and
>   `src/ase/src/windows/`, not `src/ase/src/gl/`.
> - **`requestFrame`/`scheduleTick_`/`wakePosted_` were hoisted to `PlatformImpl`**
>   (they were byte-identical across the three) so the Session installs one
>   `scheduleTick_`; the remote decorator keeps its own `requestFrame` override.
> - **The render list stays on each platform**, not moved onto the Session:
>   `makeWindow` registers into it before the Session exists (and GLX couples that
>   registration/cleanup to its X lock). The Session drives it by reference.
>   Completing the move belongs with Stage 4's `session.makeWindow`.
> - **GLX vs WGL were identical bar the wake source *and* a 1us step** (16666 vs
>   16667); the step is a `Config` field. The **dummy genuinely diverges** (no
>   draw/submit, budget self-pump, fps pacing) as anticipated — kept as Session
>   pacing config, and `DummyRenderQueue::submit` now completes each fence so the
>   shared backpressure loop resolves without a GPU.
> - **Invariant 3 (ask the surface for its target)** is met structurally: the
>   Session holds no framebuffer; each drawn surface acquires its own via
>   `getDefaultFramebuffer()` inside `frame()`. No dead `acquire()` hook was added
>   (repo forbids dead code); the draw step is written so one can slot in.
> - **Verified:** clang-cl build; `meson test` (bq/avg/bqui/btl all green, incl.
>   apptest/headlessapptest/sessiontest on the real dummy Session tick); real-GL
>   WGL via `bquiheadlesstest` (offscreen) and `testapp1` (on-screen present).
>   GLX is compile-dark on Windows — CI's Linux leg covers it.

**Goal:** a `Session` owns the loop body; GLX/WGL/**dummy** all drive through it.

**Decided:** the Session **subsumes the dummy platform** — dummy becomes "a Session
with a no-op render path." Rationale: it is the *only* automated proof the shared
Session body is correct (CI runs the real Session tick on dummy, minus the GL swap).
Dummy's `maxFps`/`maxFrames` pacing moves to a pluggable pacing hook (or up to
App's running signal, which `headlessrendertest` already uses).

**Shape:** Session owns the tick, the render list, `framesInFlight`, cadence, and
`scheduleTick`/`requestFrame`. Platform provides two hooks it already has in
spirit: `handleEvents()` (exists, `platformimpl.h:36`) and a **wake-source
descriptor** — the only real GLX/WGL divergence: `fromFd(ConnectionNumber(dpy_))`
(`glxplatform.cpp:482-488`) vs `fromMessageQueue()` (`wglplatform.cpp:416-421`);
both callbacks are the identical `handleEvents(); scheduleTick();`. The GLX/WGL
`tick` bodies are otherwise line-for-line identical (`glxplatform.cpp:400-472` vs
`wglplatform.cpp:332-404`); X-locking stays encapsulated in `present()`/
`handleEvents`, it does not surface in the loop body.

**Touches:** `glxplatform.cpp:370-499`, `wglplatform.cpp:307-433`,
`dummyplatform.cpp:50-136`, render-list ownership (`renderWindows_` per pImpl),
`App::runUntil` (`app.cpp:455-464`). Write the draw step to **ask the surface for
its target** (invariant 3), no-op-returning the default framebuffer on GL.

**Do NOT delete `Platform::run` yet** — leave it delegating to a Session so the
remote decorator still overrides it. Deletion is Stage 3.

**DoD:** all three backends drive frames through one shared Session body; behaviour
unchanged.

**Green gate:** apptest + sessiontest on dummy (CI) validate the Session
tick/re-arm/wake end-to-end. **Manual: clang-cl + `bquiheadlesstest` for GLX/WGL.**

**Riskiest stage** — structurally significant AND its substantive (GLX/WGL) half is
CI-dark. Maximum manual-gate discipline; land it in reviewable sub-commits.

## Stage 3 — Delete `Platform::run`; rehome the remote driver into `App`

**Goal:** remove `Platform::run` and the remote *platform decorator* hack.

**Coupling (why this is its own stage):** the remote layer overrides
`Platform::run` (`remoteplatform.cpp:269-293`, ignores the context, calls
`runSession`). Removing `run` from the interface (`platformimpl.h:38`) and the
`app.cpp:455` call site forces the remote branch up into `App::runUntil`, which
picks remote-vs-local and builds either a `runSession` driver or a local `Session`.
That *is* this stage — so `Platform::run` deletion and remote-rehome land together.

**Touches:** `platform.h:35`/`platformimpl.h:38` (remove `run`), `app.cpp` (remote
branch), `remoteplatform.{h,cpp}` (drop the `run` override + ignores-context hack).
**Preserve the `RemoteApp`/`runSession` seam** (`remote/session.h:84-108`) intact —
sessiontest depends on it.

**DoD:** no `Platform::run`; remote runs on the same Session seam.

**Green gate:** **sessiontest — the strongest automated signal in the whole plan**
(steps a real dummy App through `runSession`/`advance` end-to-end). Also re-run the
live MCP inject/introspect check.

## Stage 4 — Unify offscreen + shrink the public window interface

> **Partially landed on `session-refactor`.**
> - **Public window surface shrunk (done).** `needsRedraw()`/`frame()` are off the
>   public `ase::WindowImpl`/`Window` API: private virtuals reached through
>   `friend class Session`, with the concrete windows' overrides narrowed to match.
>   `present()` stays public (Stage 1).
> - **Offscreen entry point unified (done).** `makeWindow(size)` +
>   `makeOffscreenWindow(RenderContext&, size)` collapsed to one
>   `makeWindow(RenderContext& context, Vector2i size, bool headless)` on
>   `Platform`/`PlatformImpl`, every backend and the remote decorator; `headless`
>   builds the `OffscreenWindow` over the context's FBO. The context is now passed
>   for on-screen windows too (an ignored parameter on GL, but the explicit
>   window-to-context binding the design wants). The entry point stays a
>   `session.makeWindow` shorthand for later — see deferred.
> - **Render list onto the Session: DEFERRED.** Moving `renderWindows_` ownership
>   onto the Session and routing creation through `session.makeWindow` was **not**
>   landed. Two blockers, both anticipated by the stage's guard:
>   1. *Remote path.* Creation runs through `bqui::WindowImpl` in **both** local
>      and remote modes, but remote drives frames via `runSession`, not a `Session`
>      (`RemotePlatformImpl::makeSession` forwards to the *inner* platform, Stage 3).
>      A `session.makeWindow` reachable from `App::sync` in remote mode would either
>      not exist or be bound to the wrong (inner) platform, yielding real windows
>      instead of `RemoteWindowImpl`s. Keeping it behaviour-identical needs a
>      remote session-as-factory rework, past a mechanical move.
>   2. *CI-dark.* The render list is populated **only** by the real GLX/WGL
>      backends — the dummy registers no drawable surface, so apptest/sessiontest
>      never exercise it. GLX is compile-dark on Windows and its registration is
>      coupled to the X lock; the move is verifiable here on WGL only, not GLX,
>      which invariant 1 warns against for a structural loop change.
> - **Verified:** clang-cl build; `meson test` (all four suites green, incl.
>   apptest `AppWindows` + `sessiontest dynamicWindowsOpenAndCloseById`); real-GL
>   WGL `bquiheadlesstest` (offscreen via `makeWindow(headless=true)`, frames=9
>   rc=0) and `testapp1` (on-screen real window). GLX is CI-Linux-only.

**Goal:** one window entry point; render-polling off the public window surface.

**Touches:** collapse `makeOffscreenWindow(RenderContext&, size)` into
`session.makeWindow(size, headless)` everywhere (`platform.h:31`,
`platformimpl.h:34`, all backends, `remoteplatform.cpp:250`, `windowimpl.cpp:16`);
move `needsRedraw()`/`frame()` off the public `ase::WindowImpl` (`windowimpl.h:64,68`
— sole callers are the loop bodies) onto the Session-side surface; relocate the
`OffscreenWindow` present no-op (already a surface property after Stage 1).

**DoD:** one window creation entry point; `needsRedraw`/`frame` off the public
window surface.

**Green gate:** compile-wide + apptest/sessiontest wiring. **Manual: clang-cl +
`bquiheadlesstest` for real offscreen pixels.**

## Stage 5 (later) — pixel readback off the offscreen surface

Not part of the core refactor. Readback hangs off the offscreen surface; add when
needed.

## Notes

- Each stage is a commit set on `session-refactor`. Accumulate; squash/rearrange
  before any merge (iteration hygiene — do not reshape via amend/reset mid-flight).
- Before requesting human review of the whole stack: clean-context style review +
  clean-context correctness review (per AGENTS.md), plus a docs-freshness pass
  (this file + `render-session.md`).
