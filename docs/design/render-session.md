# Design: the render session, and where `Platform::run` belongs

> **Status: partially built (Stages 2-4 landed on `session-refactor`, one piece
> of Stage 4 deferred).** An `ase::Session` owns the frame-loop body and GLX, WGL
> and the dummy backend all drive frames through it; `Platform::run` is gone and
> `App` picks the driver (local `Session` vs remote `runSession`). Offscreen has
> collapsed to `makeWindow(context, size, headless)`, and `needsRedraw()`/`frame()`
> are off the public window surface (Session-private virtuals). Still the target:
> one context per session, and the render list *owned by* the Session. See
> [`render-session-plan.md`](render-session-plan.md) for what each stage does and
> what remains. Sits above [`runloop.md`](runloop.md), which designs the loop
> *mechanism* this note builds on.
>
> What landed so far: the Session binds the platform's `RunLoop` + `RenderContext`
> + its render list and drives frames (clock/cadence, GPU backpressure, on-demand
> re-arm). `App` constructs and runs it (App owns the session). The **render list
> still lives on each platform** (its `makeWindow` registers into it, and GLX
> couples that to its X lock), driven by reference — moving ownership onto the
> Session is the deferred piece of Stage 4. The dummy is "a Session with a no-op
> render path": it registers no surface, and its `maxFps`/`maxFrames` are the
> Session's headless pacing in place of an OS event source.

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
- The net: the mechanism got abstracted (`RunLoop`) while the session it feeds did
  not.

## The model: a render session

Introduce the missing layer — a **session** (`Renderer`/`RenderSession`, name TBD)
that binds a `RunLoop`, a `RenderContext`, and its windows, and drives frames:
subscribe to the RunLoop's frame timing, draw the windows that `needsRedraw()`,
present. Then:

- **One context per session**; a window is a **surface of its session's context**,
  created bound to it.
- **`Platform` reduces to OS backend + factory** — make windows, make contexts,
  pump OS events, provide the RunLoop. It no longer has `run`.
- **`App` owns a session** (widgets and reconciliation are unchanged — that stays
  `App`'s job).

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

## Re-layering

- **Platform** — OS backend + factory; makes windows and contexts, pumps OS
  events, provides the RunLoop.
- **RunLoop** — the loop mechanism ([`runloop.md`](runloop.md)); OS-owned or
  self-driven.
- **RenderContext** — the device.
- **Session** — binds RunLoop + context + windows; drives frames.
- **App** — widgets and reconciliation; owns a session.

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

## Open questions

- Who constructs and owns the `Session` — `App` directly, or a platform/context
  factory that also supplies the RunLoop.
- Where `present` and pixel-readback live — on the session, or on the
  window-surface it renders.
- Whether window creation moves onto the `RenderContext` (surface-of-a-device,
  matching Vulkan/Metal) or stays on the `Platform` with an explicit context
  binding passed at creation.
- Naming: `Session` vs `Renderer` vs other.

## Non-goals

- No change to the widget / frame-reconciliation model; that remains `App`'s.
- `btl::RunLoop` stays as the loop mechanism; this note is the layer above it.
