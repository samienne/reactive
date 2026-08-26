# bqui — agent notes

*Last verified against `55f2e55` (2026-08-15), for the headless offscreen rework.*

Internals, entry points, and traps for the UI toolkit. Concepts and usage are in
`readme.md`; project-wide conventions are in the top-level `docs/`. This file is
the index for `bqui`; split topics into a `.agent/` folder here if it outgrows
one file.

## Widget pipeline

`Widget → Builder → Element → Instance`, each stage lowering the description
toward drawable state (`widget/`):

- `AnyWidget = Widget<std::function<AnyBuilder(BuildParams)>>` — the user-facing,
  type-erased description.
- `Builder` receives `BuildParams` (theme, environment) and produces size hints.
- `Element` (`AnyElement = Element<AnySignal<Instance>>`) wires up signals and
  receives the resolved size.
- `Instance` is the realised widget: its render tree, input areas, hit regions.

Modifiers are `AnyWidgetModifier` joined with `operator|`. They exist at each
pipeline level (widget/builder/element/instance modifiers in `modifier/`), so a
modifier can act wherever it needs to.

## Shapes

Two layers (`shape/`): `avg::ShapeFunction` is the animatable engine;
`bqui::shape::Shape<T>` is an **rvalue-chained** builder wrapping a signal of it.
Transforms/composition return a new `Shape`; `fill`/`stroke`/`fillAndStroke`
terminate to `AnyWidget`. Transforms are **paint-time and never affect layout**
(the layout axis is separate — `docs/decisions.md`).

## Layout, environment, animation

- Layout: size hints (min/natural/stretch) negotiated by containers; `gravity`
  aligns within allocated space. Size hints are signals.
- Environment: a typed, scoped store threaded through the tree (`provider/`,
  `modifier/setparams.h`); `Theme` is the common parameter.
- Animation: `withanimation.h` (guard or lambda form) marks changes to animate.

## The app loop (`app.cpp`)

**A `Window` is a handle; the widget is supplied at `addWindow`.** A `Window` is
a `std::shared_ptr<WindowData>` (`src/windowdata.h`): its copies are one window
and share the data — identity, title, close callbacks — which lives as long as
any copy does, across a remove and a later re-add. The window holds none of its
own contents. `WindowData` links back to the app only weakly, so it is a
strong-leaf; that is what lets a widget capture the very `Window` that owns it
without a cycle (see `docs/decisions.md`).

**One imperative collection says which windows exist.** `AppDeferred::windows_`
is a `bq::signal::SharedVector<Window>`. `App::addWindow` appends to it,
`App::removeWindow` and `Window::close` remove from it, and `App::getWindows` /
`App::getWindowsSignal` read it — as a snapshot and as a signal respectively. The
signal is an observation for a UI that wants to follow the set; it is not the
source, and nothing derives the windows from it.

`App::addWindow(window, widget)` does two things: it adds the window to the
collection (rejecting a duplicate or a window already in another app), and it
enqueues the `(window, widget)` pair on `pendingMounts_`, a mutex-guarded queue,
because an OS window must be built on the app thread. So `addWindow` is safe from
any thread; the widget waits there for the run loop.

`App::run` keeps a live `WindowBridge` per window, `windowBridges_`, keyed by window
id. A `WindowBridge` (which absorbed the old `WindowGlue`) owns everything for one
window: its `ase::Window`, painter, per-window signal contexts, input state, and
the **widget** it was mounted with. It holds the window's `WindowData` too, but
not its identity — the data outlives the impl. Under `App::headless` the
`ase::Window` is an `ase::OffscreenWindow` — the real backend rendering into an
FBO with nothing shown — but the impl treats it as an ordinary window: same
`clearWindow`/`paintToWindow`/`presentWindow` path, driven by the same platform
run loop (present is a no-op for an offscreen window). Each frame the loop
**syncs** the
impls to the collection with a plain O(n) set-diff: it reads `getWindows()`,
drains `pendingMounts_` and mounts a `WindowBridge` for any window in the
collection that has none, and tears down any impl whose window has left. A window
that survives an edit keeps the impl it already had — it is never rebuilt. A
pending widget whose window has since left (or is already mounted) is dropped; a
re-add re-supplies one. `AnimationGuard` walks `windowBridges_`. The no-signal
`run()` overload derives its `running` signal from `getWindowsSignal` and stops
when the collection is empty.

The sync replaced an earlier signal-driven design (see `docs/decisions.md`): the
window set used to be an `ArraySignal<Window>` mapped to one glue per identity
and joined, with a `collect()` reconcile off the joined signal and a second
`addWindowArray` path. There is no array, no join, and no `addWindowArray` now;
the plain set-diff is all the matching a by-value identity needs.

A window belongs to one app: `App::addWindow` rejects a window whose data already
names a live app, and `removeWindow` clears that reference so a closed window can
be opened again. Without that a window could be open in one app and closable only
from another.

**Remote mode is an `App` branch that attaches a driver — no decorator, no window
wrapper.** With a remote endpoint set, `runUntil` builds a `remote::RemoteApp`
(`sync` and `liveWindows` over `windowBridges_`) and attaches a
`remote::RemoteDriver` (`include/bqui/remote/remotedriver.h`) over the platform's
`pause()`/`step()` *before* `platform.run(frameCallback)`. The driver connects the
client socket, registers it on the platform's run loop, and — in client-driven
mode — holds a `pause()` token so the client owns the clock and each `advance`
steps the platform's one loop directly through that token
(`PauseToken::step` → `frameCallback` + `renderDirtyWindows`) — the single frame
path for every backend, dummy included, so App no longer sets `RemoteApp::step`
(it stays the token-less fake-window fallback). The frame path is otherwise
identical to local; the only branch is whether the driver is attached. The window
glue (`WindowBridge`) itself implements `remote::RemoteWindow`, so introspection and
event injection are a capability of the glue — available for any window — and the
driver addresses each glue by its `UniqueId`. The transport and JSON-RPC protocol
are reused unchanged (see `docs/decisions.md`).

The impls are released by a scope guard in `run`, not at the end of it. They
outlive the call — they are the app's — but the `ase::Platform` and
`ase::RenderContext` they are made of do not, so a run that ends by exception
must not leave one behind.

### How `Window::close()` reaches the collection

`WindowData` carries the window's `btl::UniqueId` and a
`std::weak_ptr<AppDeferred>`, stamped in by `App::addWindow`. `close()` locks the
weak app and calls `AppDeferred::removeWindow(id)`, which leaves the collection;
the impl is torn down by the next sync, not inline. Weak, because a window
outlives its app whenever a widget or a callback still names one: `close()` on a
window whose app is gone — or that was never added — finds nothing to lock and
returns.

The back-reference is to the app rather than to the impl on purpose: it makes
`close()` work before a window is mounted (and off the app thread), which the
non-headless `windowtest.cpp` exercises, and it keeps `WindowData` free of any
reference to the impl — the strong-leaf property the cycle-safety depends on.
There is no separate `WindowHandle`; the `Window` is the handle now, and folding
its weak app reference into `WindowData` is what deleted it.

### Ordering

Two rules hold the frame phase together, and both are load-bearing rather than
tidiness:

- **A window's signals are updated in the frame phase, never from an input
  handler.** The handler marks the window for redraw and returns; the platform
  runs the frame after `App::run`'s callback has synced the impls to the
  collection, so a window that has just been removed is torn down before
  anything updates it.
- **An impl is released only after the main render queue has caught up.** An
  in-flight frame holds the window's framebuffer, which holds the window by
  reference, so the sync calls `mainQueue.finish()` before it destroys a
  departed impl.

Removal itself never evaluates a signal. `WindowBridge`'s close callback invokes
the window's own callbacks and then removes it, and a removal writes the
`SharedVector`, whose publication only sets an input. `Window::close()` (a widget
button, say) writes the same `SharedVector` and no more; the callbacks are the
title bar's path alone.

**An impl drives its window's own signals.** Everything it updates per frame is
the window's *own* title signal and the widget it was mounted with — not a
per-frame read of the collection. There is therefore no departed-key hazard on
this path: the collection can change and a surviving impl keeps driving what it
already holds.

The teardown ordering is the one subtlety worth stating: the sync builds the new
complete impl set, swaps it into `windowBridges_`, and *then* destroys the departed
impls, because destroying a window runs event handlers that reach back into the
live set, which must be the new one by then.

`AnimationGuard` transacts every impl from its constructor and destructor, and
`withAnimation` is called from input handlers. A handler that both animates and
removes a window transacts the departing window synchronously, which is harmless:
the value is stable and the close path only invokes the window's own callbacks
and returns.

`test/apptest.cpp` drives the collection through a scripted `running` signal on
the headless dummy backend (macOS CI only), covering mount/remove/close, the
impl lifecycle, and the cycle-safety of a close button that captures its owning
`Window`. `test/windowtest.cpp` covers the collection and the handle's lifetime
without a backend at all, so it runs everywhere. Run `testapp1` to exercise the
rest — its extra windows are app-owned, and each close button captures the
owning `Window`.

## Traps

- `Widget`/`Element`/the modifiers have **no `extern template` declaration and
  no explicit instantiation** — every translation unit instantiates them
  implicitly. GCC cannot export a specialization these classes name inside their
  own bodies, and a Release build hides that, so don't add one back; the
  gcc-12/clang-11 Debug legs are what catch it (see `docs/conventions.md`).
- Shape builder methods are `&&`-qualified — call other `&&` overloads via
  `std::move(*this)`. A parameterless shape can't use `merge()`; it's built as a
  constant (the `if constexpr (sizeof...(Ts) == 0)` branch in `shape/shape.h`).
- Builder-style template APIs are guarded by an instantiation smoke test
  (`test/shapetest.cpp`) — extend it when adding builder methods.
- **Known and unfixed: a child's size hint is instantiated twice under every
  `layout()` container.** The hint fan-in reads each child's `getSizeHint()`,
  and `handleGravity` reads it again when that child's element is built, so the
  hint chain is evaluated once per child per pass instead of once. Fixing it
  means having the builder hand out one shared hint signal rather than a fresh
  one per call — builder plumbing, not layout — which is why it is recorded here
  instead of patched at the call site.
- Geometry recovered from input areas (`test/layouttest.cpp`) says nothing about
  the render tree: a node placed wrongly, one that cannot be drawn at all, or
  one paired with the wrong sibling across an update all leave the input areas
  intact. Anything that only manifests when drawing belongs in
  `test/rendertreetest.cpp`, which splices realised instances the way the window
  loop does and asserts on the `avg::Drawing` that comes out.

For cross-cutting rules (the `Any` convention, include-dir firewall, symbol
visibility) see `docs/conventions.md`; do not restate them here.
