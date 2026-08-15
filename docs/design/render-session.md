# Design: the render session, and where `Platform::run` belongs

> **Status: design settled, not built.** The session model below is the agreed
> target; the four earlier open questions are now decided (see *Decisions*), and
> present is redesigned out of the command stream (see *Present is not a
> command*). Implementation lands with the inspector/remote work (#96 lineage),
> not before it. Current code as of master `dcd9aa5` (2026-08-15). Sits above
> [`runloop.md`](runloop.md), which designs the loop *mechanism* this note builds
> on.

## Purpose

Decide who owns the frame loop. Today `Platform::run(RenderContext&, callback)`
drives frames, and that one method quietly carries three things that don't belong
together: the OS backend, the rendering device, and the set of windows. Pulling
them apart removes a false generality, makes the window/context relationship
honest (which matters the moment a non-GL backend arrives), and gives iOS-style
platforms a place to fit.

## The mismatch today

`Platform` is a factory and an OS backend: `makeWindow`, `makeRenderContext`,
event pumping, and — via [`btl::RunLoop`](runloop.md) — the loop mechanism. But
`run(context, callback)` also makes it the **frame-session driver**, tying the
loop, one context, and the windows together and pushing frames. Four symptoms:

- **False multi-context generality.** `run` takes exactly one `RenderContext`,
  and `makeOffscreenWindow` takes one too — so the API *reads* as if several
  contexts are supported, but a single `run` with a single event loop can only
  ever drive one. The flexibility implied by threading contexts around isn't
  real.
- **The window is already bound to a context, GL just hides it.** A window's
  default framebuffer is a marker object the GL backend recognises as "the
  swapchain," and GL framebuffers are context-agnostic — so windows *look*
  detachable from any context. That is a GL-ism. On Vulkan/Metal/D3D a swapchain
  is created **on a device** and is bound to it; you cannot hand it to another.
  So the window-to-context binding is real and will tighten. A painter on a
  different context for the same window does not make sense.
- **`Platform::run` is the wrong owner for iOS.** There the OS owns the main loop
  (you register a display link); the platform does not "run." `btl::RunLoop`
  already abstracts *that* — OS-owned or self-driven — but the **session** (loop +
  context + windows) is still stuck inside `Platform::run`, which assumes the
  platform drives it.
- **Present is a routable command, not a bound operation.** A present is pushed
  into the command buffer carrying a `Window&` (`Painter::presentWindow` ->
  `commandBuffer_.pushPresent`), and the target context is only recovered at drain
  time by walking `getImplOfType` for the concrete window type. So the command
  *reads* as portable across contexts, but a window's swapchain lives on exactly
  one device — the same false generality as `run(context)`, one layer lower. See
  *Present is not a command*.
- The net: the mechanism got abstracted (`RunLoop`) while the session it feeds did
  not.

## The model: a render session

Introduce the missing layer — a **`Session`** (name decided; see *Decisions*)
that binds a `RunLoop`, one `RenderContext`, and its windows, and drives frames:
subscribe to the RunLoop's frame timing, draw the surfaces that `needsRedraw()`,
present each drawn surface. Then:

- **One context per session**; a window is a **surface of its session's context**,
  created bound to it. The `Session` holds those bound surfaces.
- **Present is a surface operation, not a command** — the `Session` triggers
  `present()` on each surface it drew, in sequence after submit; the surface knows
  its own window and context. See *Present is not a command*.
- **`Platform` reduces to OS backend + factory** — make windows, make contexts,
  pump OS events, provide the RunLoop. It no longer has `run`.
- **`App` owns exactly one `Session`** (widgets and reconciliation are unchanged —
  that stays `App`'s job).

## What it resolves

- **Honest single context** — one per session, no pretence otherwise.
- **Genuine multi-context, if ever wanted** — as **multiple sessions** on the one
  RunLoop, each with its own context. That is the real generality; the current
  false kind (contexts threaded through one `run`) is dropped.
- **Explicit window-to-context binding** — a window is a surface of a context,
  which is exactly the shape Vulkan/Metal/D3D need.
- **iOS fits** — the OS-driven RunLoop drives the session; there is no `run` for
  the platform to own.
- **`needsRedraw()` / `frame()` move off the `WindowImpl` interface.** They are on
  the base interface today only so the one unified loop can drive every window
  polymorphically; with a session that owns the render relationship, they belong
  there, not on the window's public surface.
- **Offscreen collapses to "one more surface on a context."** `makeWindow(size,
  headless)` unifies (no separate `makeOffscreenWindow`), and the session drives
  an offscreen window exactly like a real one; pixel readback hangs off the
  session/window-surface, not the base window.
- **The present type-switch collapses.** With present as a virtual on the bound
  surface, the COM-style `getImplOfType` bubble-up across
  `Gl`/`Glx`/`WglRenderContext` disappears into ordinary dispatch, and the
  offscreen no-op stops being a special case checked inside a command handler — it
  is just the offscreen surface's `present()`.

## Re-layering

- **Platform** — OS backend + factory; makes windows and contexts, pumps OS
  events, provides the RunLoop.
- **RunLoop** — the loop mechanism ([`runloop.md`](runloop.md)); OS-owned or
  self-driven.
- **RenderContext** — the device. No longer in the present business.
- **Surface** — a window bound to a context; knows how to `present()` itself (swap
  for a real window, no-op/readback for offscreen).
- **Session** — binds RunLoop + context + its surfaces; drives frames and triggers
  present.
- **App** — widgets and reconciliation; owns one session.

## Present is not a command

Present is currently a command in the buffer: `Painter::presentWindow` pushes a
present carrying a `Window&`, and the render context's present callback recovers
the concrete window type by walking `getImplOfType` at drain time. That bundles
two separable things:

1. **Sequencing on the queue's timeline** — the swap must happen after that
   frame's draw commands have executed. This is real and stays.
2. **A routable work item that carries a window** — pushable onto any queue, with
   the context resolved only at drain time. This is the leak: a window's swapchain
   belongs to exactly one context, so "present window W" is not portable.

Keep (1), drop (2). Vulkan already draws this line: there is no recorded present
command; you submit draw work, then call `vkQueuePresentKHR` on a swapchain bound
to the device, gated by a render-finished semaphore. GL only lets present pose as
a command because `SwapBuffers`/`glXSwapBuffers` implicitly flush the current
context. Designing present out of the command buffer now makes the model honest
and matches the backends to come.

Present becomes a virtual on the **surface** (the bound window+context pair the
session holds): `SwapBuffers`/`glXSwapBuffers` for a real window, a no-op or pixel
readback for an offscreen one. The `getImplOfType` present chain across the render
contexts collapses into ordinary virtual dispatch, and the offscreen no-op becomes
a surface property rather than a special case inside a command handler.

Sequencing (verified against the code): the GL render queue runs on a dedicated
render thread, and the GL context is current only on that thread — so the swap
must execute there, after that window's draw commands, not on the loop/session
thread. The mechanism already exists: `RenderContext::dispatch` posts a task onto
the render thread, FIFO-ordered behind submitted command buffers. So the session,
right after submitting a frame's draw buffer, dispatches the surface's `present()`
onto that same queue; it runs after the draws on the context thread, exactly where
the in-buffer present command runs today. This is *not* gated on the completion
fence — that fence signals GPU-complete and stays for `framesInFlight` backpressure
only; gating present on it would cost a frame of latency. Two backend caveats the
surface must honour: the GLX swap holds the platform X lock and `XSync`s, and the
WGL swap relies on the window's DC being current from its draws this frame (an
out-of-band present for a window that drew nothing must make its DC/context current
first).

## Forward-compatibility (validated: Vulkan, D3D12, Metal, Android)

The model was checked against the four backends we expect to add. The core holds
on all of them: one context = one device (`VkDevice` / `ID3D12Device` /
`MTLDevice`), a window is a swapchain/drawable **created on and bound to** that
device (not transferable), present is a surface/queue operation and never a
recorded render command (`vkQueuePresentKHR`, `IDXGISwapChain::Present1`, Metal
`presentDrawable:`), and the loop is OS-owned or self-driven (iOS `CADisplayLink`,
Android `Choreographer`). The **Platform-makes-native-window / Session-binds-it**
split is reinforced by Android, where the `ANativeWindow` outlives the swapchain
and is reused to build a new one after loss — exactly this seam. So stopping short
of a `RenderContext::makeWindow` factory is safe: nothing forces window creation
onto the device; the device-bound object is the swapchain, which lives behind the
`Surface` (where present already lives).

Two things real backends need that GL hides — both are `Surface`-interface shape,
neither disturbs the Session/Platform/Context split, and both are no-ops on GL.
Reserve them now so the GL-only stages don't cast a shape that has to be undone:

- **`present()` returns a status, not `void`.** Every non-GL backend can fail
  present and *demand swapchain recreation* — `VK_ERROR_OUT_OF_DATE_KHR` /
  `VK_SUBOPTIMAL_KHR` / `VK_ERROR_SURFACE_LOST_KHR`, `DXGI_ERROR_DEVICE_REMOVED`,
  Android surface-destroyed, Metal nil-drawable. GL's swap never reports this.
  Shape `present()` with a status return from the start (GL always returns `Ok`),
  because retrofitting a `void` return touches every call site. The `Session` owns
  the **surface-recreation path**, triggered by that status or by a
  resize/rotation/lifecycle event.
- **A `Surface::beginFrame()` / `acquire()` hook is coming.** Modern backends are
  **acquire -> render -> present**: the frame's render target comes *from the
  surface* per frame (`vkAcquireNextImageKHR`, `GetCurrentBackBufferIndex`,
  `nextDrawable`) and acquire can fail like present. Write the Session's draw step
  to **ask the surface for this frame's target** rather than assume a fixed
  framebuffer, so acquire slots in later. On GL it is a no-op returning the default
  framebuffer. Keeping acquire on the surface (not the session) is also what keeps
  per-backend synchronization (semaphores/fences) from leaking into the Session.

Two wording guards: present is "the surface sequences present relative to its own
submit," not "the session presents strictly after submit" (Metal records present
into the command buffer before commit); and the Session must tolerate **having no
valid surface** (mobile backgrounding destroys it) rather than assuming a surface
is always drawable. None of this is implemented until a non-GL backend lands.

## Relationship to the current (interim) code

What just landed on master is the interim, on the current GL model: offscreen via
`Platform::makeOffscreenWindow(RenderContext&, size)`, `Platform::run(RenderContext&,
callback)`, `needsRedraw()`/`frame()` on `WindowImpl`, and a single unified
per-platform render list. It works and does not box this in — an offscreen window
is already "one more surface on a context," which the session model keeps — but it
carries the mismatch above.

The session refactor supersedes the interim and belongs **with #96**, not before
it: the inspector/remote layer drives windows headlessly, renders offscreen, and
(eventually) reads back pixels — which *are* the session's concerns. Doing the two
together avoids reworking #96's loop twice.

## Decisions

- **`App` owns exactly one `Session`.** `App::runUntil` is already the de-facto
  session constructor (it makes the context and threads it into `run`); it now
  constructs a `Session` and hands it the windows, replacing the `platform.run`
  tail. The `Platform` loses `run`.
- **Present lives on the surface, driven by the session; the `RenderContext` gets
  out of the present business.** Pixel readback hangs off the offscreen surface
  for the same reason. See *Present is not a command*.
- **Window creation goes through the `Session`, against its context**
  (`session.makeWindow(size, headless)`), which unifies the offscreen entry point
  and records the window->context binding. We stop **short** of reshaping
  `RenderContext` into a `makeWindow` surface-of-a-device factory: the `Platform`
  still makes the raw OS window and the `Session` pairs it with the context. The
  full surface-of-a-device move waits for a real Vulkan/Metal/D3D backend.
- **Name: `Session`.** `Renderer` overloads the drawing sense.

## Still open

- Nothing structural. The one prior open question — whether a non-GL backend
  forces a `RenderContext::makeWindow` reshape — is answered by the
  forward-compatibility check above: session-holds-the-binding is sufficient, and
  swapchain create/recreate lives behind the `Surface`. A `RenderContext::makeWindow`
  would be convenience, not necessity.

## Non-goals

- No change to the widget / frame-reconciliation model; that remains `App`'s.
- `btl::RunLoop` stays as the loop mechanism; this note is the layer above it.
