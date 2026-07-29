# Design: the frame model - windows, vsync, and on-demand rendering

> **Status: design (ahead of the code).** This pins the model we build the `ase`
> frame layer and `bqui`'s `FrameClock` against; the API contract will be Doxygen
> in the headers. It sits on top of [`runloop.md`](runloop.md) and does not change
> `btl::RunLoop`. Verified against the `runloop-design` branch (2026-07-29).

## Why

The desktop loop today busy-spins: the frame tick re-posts itself every iteration,
`requestFrame` is an empty stub, and the GPU fence is awaited with a blocking
`wait()` on the loop thread. So the loop never sleeps, never paces to the display,
and cannot host a platform that *pushes* frames (iOS, Android, web). None of the
"only render when needed" machinery was ever built.

This model fixes all of that with one shift: **frames are driven by a per-window
vsync source that the app arms on demand, not by a platform-owned loop that calls
the frame body.** Everything else - idle sleep, vsync pacing, async GPU
completion, and platform-pushed frames - falls out of that.

## What a window is

The desktop assumption baked into "the app creates N OS windows, each with an
always-present surface" is false on mobile in two independent ways, so the model
splits the window from its surface:

- **Logical window** - app-owned, stable identity, part of the reactive window set
  the `App` declares. The app says *what it wants*; it has its own signal context.
- **Surface** - platform-owned renderable (swapchain / `CAMetalLayer` /
  `ANativeWindow`). It may be **absent**, and it **attaches and detaches** on a
  lifecycle the platform controls.

The platform **reconciles** declared windows against available surfaces:

| platform | creation | surface lifetime |
|---|---|---|
| desktop | app-driven, 1:1 | always present while the window lives |
| iOS | system hands a `UIWindow` per `UIScreen` | drawable attaches/detaches; extra windows are scenes (later) |
| Android | one `Surface` per `Activity` | `surfaceCreated` / `surfaceDestroyed` |
| web | one `<canvas>` | present while mounted |

When the surface is absent (backgrounded), the **logical window persists** and its
vsync goes quiet, so rendering simply pauses; it resumes when the surface
reattaches. This is why *declare-and-reconcile* beats imperative window creation:
the platform matches intent against reality instead of assuming it can conjure OS
windows.

## vsync is a per-window, armable source

Vsync is a `RunLoop` source (see [`runloop.md`](runloop.md)), in the same category
as a socket or a timer - registered on the loop, its callback runs on the loop
thread, and the idle loop blocks on it alongside everything else. Two things make
it special:

1. **It is per-window** - really per *display*, surfaced through the window, which
   knows its display. Two monitors at 60/144 Hz each pace their own window. A
   window owns its vsync source.
2. **It is armable / maskable.** Unlike a socket ("ready when data arrives"), a
   vsync tick is only wanted when a frame is pending. `requestFrame` arms it; after
   a frame renders with nothing more wanted, it disarms and the loop stops waking
   on it. That arm/disarm *is* the on-demand mechanism.

Its `btl` backing differs per platform, but the frame scheduler sees one thing -
"call me at the next frame boundary for this window":

| platform | vsync backing |
|---|---|
| Windows | a **waitable-swapchain HANDLE** (the `addReadable(HANDLE)` kind the loop already has) |
| GLX | a **refresh-interval timer** (no vsync fd to wait on) |
| iOS / Android / web | a **native callback** (`CADisplayLink` / `AChoreographer` / `requestAnimationFrame`) that `post`s into the adapter loop |

No new loop type: `ase` registers a source and adds a scheduling *policy*; it never
owns iteration, so the mobile `RunLoopImpl`-as-adapter story holds.

## The cadence

Per window, the frame scheduler is a two-state machine:

```
        external change (observe fires -> requestFrame)
  IDLE ------------------------------------------------> ACTIVE
   ^                                                       |
   |  render result: not animating (re-arm observe)        | render result:
   +-------------------------------------------------------+ still animating
                                                           (requestFrame)
```

- **idle**: `observe` is armed on the window's root signal; the loop sleeps. The
  window renders nothing.
- **idle -> active**: an external leaf invokes `observe`'s stored callback, which
  calls `requestFrame` and arms the window's vsync.
- **active tick** (on a vsync fire): advance the window's frame-time signal,
  evaluate the graph to build the render tree, render it, and submit with an
  async fence. The **render result reports whether animations are still running**.
- **active -> active**: still animating -> `requestFrame` again (stay armed).
- **active -> idle**: nothing animating -> **re-arm `observe`**, disarm vsync,
  sleep.

Two drivers, cleanly separated: **`observe` drives idle -> active** (discrete
external changes), and the **render result drives active -> active** (animation).
`observe` alone cannot drive animation - an animated value depends on the
frame-time signal, which only advances when we render, so there is nothing for
`observe` to fire on. The render result closes that loop.

## `signal::observe`

`observe` is the idle wakeup, and the whole on-demand story rests on it:

- It walks the graph from the window root and registers a callback on every leaf
  that can change from **outside** - `signal::input`, stream-to-signal connectors,
  async-backed signals.
- On an external change the leaf invokes the callback **immediately, without
  evaluating the graph**. So the callback is cheap (just `requestFrame`), and a
  burst of changes coalesces into one armed frame.
- It is **one-shot and re-armed on each idle entry**. This is not a limitation but
  the correct choice for a *dynamic* graph: `forEach` and conditionals reshape
  which leaves are external, and re-walking on idle re-discovers them. A persistent
  subscription would go stale. During active mode `observe` is not armed at all -
  every frame evaluates fresh - so reshaping there is picked up for free.

Because `observe` is armed **only while idle**, the frame-time advance we do each
active tick cannot masquerade as an external change and re-arm us into a
busy-loop.

**Ordering rule (a real correctness dependency on single-threadedness):** re-arm
`observe` *inside* the going-idle step of the tick, before yielding to the loop. An
input or stream delivery only runs as a source callback *after* the current tick
returns, so as long as re-arm is part of the tick, the next external change is
always caught. Re-arming after yielding would open a lost-wakeup window.

**What to verify.** The failure mode is not a leaf type - it is a *combinator that
does not forward `observe` registration to its inputs*. If `map` / `join` /
`forEach` / conditional register on themselves but not recursively on their
upstream external leaves, a change below them fires nothing and that subtree
silently freezes while idle. The test matrix is therefore **{`signal::input`,
stream->signal, async-backed} x {reached through `map`/`join` and the dynamic
`forEach`/conditional}**, asserting the callback fires exactly once per change.

## The GPU fence is async, not awaited

The frame submits a command buffer with a fence and **posts** its completion back
onto the loop rather than blocking on it: the fence's continuation runs on the loop
thread via the threadpool -> `post` seam ([`runloop.md`](runloop.md)). The loop
keeps servicing input and vsync while the GPU works. Back-pressure is an in-flight
cap: the fence continuation is what releases a slot and requests the next frame, so
the pipeline never queues more than N frames deep and never stalls the loop thread.
The cost is a touch of latency the frame after a stall actually resolves - an
acceptable trade for a responsive loop.

## Per-window independence

Each window owns its **surface, signal context, vsync source, `observe`
registration, `requestFrame` state, and frame-time signal**. Nothing is shared, so
windows on different displays tick at different rates and idle independently. The
"one fused app frame" of the current `App` (evaluate the graph once, advance all
windows together) is a **v1 collapse** - drive every window off one clock - kept
only because it is simpler for same-refresh desktop. The per-window signal contexts
already exist, so relaxing the collapse into true independence is additive, not a
rewrite. The frame-time signal is per-window from the start precisely so that
collapse is a policy choice, not a structural one.

## What lives where

- **`btl::RunLoop`** - unchanged. Readable/timer/post is enough; the HANDLE source
  already covers a waitable swapchain. Frames and vsync stay out of it.
- **`ase`** - the per-window vsync source and the frame scheduler (arm/disarm,
  render, async fence). On the push platforms the vsync callback feeds the adapter
  loop.
- **`bqui`** - `FrameClock` as the policy over the scheduler (`observe` wiring, the
  cadence state machine, `requestFrame`, animation continuation), and the
  window/surface reconciliation in `App`. The frame callback's result reports
  "wants another frame" (animating), distinct from the old "shut down" bool.

## First implementation and non-goals

- **Desktop first**, both backings: the waitable-swapchain HANDLE on Windows and
  the refresh-interval timer on GLX. The iOS/Android/web vsync sources are deferred
  behind the same source abstraction; the point of the abstraction is that they
  slot in without reshaping the scheduler.
- **v1 may keep the single-clock collapse** (one vsync drives the app frame) while
  building everything per-window underneath, so the multi-refresh, per-window-paced
  case is *designed for* but not necessarily *shipped* first.
- The window/surface split is introduced now, because it is what makes the model
  mobile-ready and it degrades to "surface always present" on desktop at no cost.
