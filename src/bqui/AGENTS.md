# bqui — agent notes

*Last verified against `77e391f` (2026-07-22).*

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

**One imperative collection says which windows exist.** It lives in
`src/windowlist.h` as a `bq::signal::SharedVector<Window>` behind a thin
`WindowList`. `App::addWindows` appends to it, `App::removeWindow` and
`Window::close` remove from it, and `App::getWindows` / `App::getWindowsSignal`
read it — as a snapshot and as a signal respectively. The signal is an
observation for a UI that wants to follow the set; it is not the source, and
nothing derives the windows from it.

`App::run` keeps a live `WindowGlue` per window, `windowGlues_`, keyed by window
id. A `WindowGlue` owns everything for one window: its `ase::Window`, painter,
per-window signal contexts, and input state, and it holds the window itself as a
plain value. Each frame the loop **syncs** the glues to the collection with a
plain O(n) set-diff: it reads `getWindows()`, builds a glue for any window that
has none, and tears down any glue whose window has left. A window that survives
an edit keeps the glue it already had — it is never rebuilt. `AnimationGuard`
walks `windowGlues_`. The no-signal `run()` overload derives its `running` signal
from `getWindowsSignal` and stops when the collection is empty.

The sync replaced an earlier signal-driven design (see `docs/decisions.md`): the
window set used to be an `ArraySignal<Window>` mapped to one glue per identity
and joined, with a `collect()` reconcile off the joined signal and a second
`addWindowArray` path. There is no array, no join, and no `addWindowArray` now;
the plain set-diff is all the matching a by-value identity needs.

A window belongs to one app: `WindowList::add` rejects a window whose handle
already names a live list, and `remove` clears the handle so a closed window can
be opened again. Without that a window could be open in one app and closable
only from another.

The glues are released by a scope guard in `run`, not at the end of it. They
outlive the call — they are the app's — but the `ase::Platform` and
`ase::RenderContext` they are made of do not, so a run that ends by exception
must not leave one behind.

### How `Window::close()` reaches the list

Through `WindowHandle`, which carries the window's `btl::UniqueId` and a
`std::weak_ptr<WindowList>` behind a shared control block. `WindowList::add`
stamps the list into every handle it takes, so a copy of a window taken *before*
it was added closes it just as well. Weak, because a window outlives its app
whenever a widget or a callback still names one: `close()` on a window whose app
is gone finds nothing to lock and returns.

`WindowList` is its own object rather than a member of `AppDeferred` for exactly
that: it is what a window may hold, and holding `AppDeferred` weakly would mean
holding the render state and the glue vector too.

The handle is separately constructible so that a widget inside the window can
hold one. A window that captured itself would own the widget that owns it, and a
widget that captured the `App` would close a cycle through the collection;
`WindowHandle` is what neither of those is.

### Ordering

Two rules hold the frame phase together, and both are load-bearing rather than
tidiness:

- **A window's signals are updated in the frame phase, never from an input
  handler.** The handler marks the window for redraw and returns; the platform
  runs the frame after `App::run`'s callback has synced the glues to the
  collection, so a window that has just been removed is torn down before
  anything updates it.
- **A glue is released only after the main render queue has caught up.** An
  in-flight frame holds the window's framebuffer, which holds the window by
  reference, so the sync calls `mainQueue.finish()` before it destroys a
  departed glue.

Removal itself never evaluates a signal. `WindowGlue`'s close callback invokes
the window's own callbacks and then removes it, and a removal writes the
`SharedVector`, whose publication only sets an input.

**A glue holds its window as a plain value.** `window_` is the `Window` the app
handed the glue when it opened the window, and the glue never replaces it.
Everything it drives per frame is the window's *own* title and widget signals,
which are the window's, not a per-frame read of the collection. There is
therefore no departed-key hazard on this path — the collection can change and a
surviving glue keeps driving the value it already holds.

The teardown ordering is the one subtlety worth stating: the sync builds the new
complete glue set, swaps it into `windowGlues_`, and *then* destroys the departed
glues, because destroying a window runs event handlers that reach back into the
live set, which must be the new one by then.

`AnimationGuard` transacts every glue from its constructor and destructor, and
`withAnimation` is called from input handlers. A handler that both animates and
removes a window transacts the departing window synchronously, which is harmless
now: the value is stable and the close path only invokes the window's own
callbacks and returns.

`test/apptest.cpp` drives the collection through a scripted `running` signal on
the headless dummy backend, but the backend never runs a window's frame callback,
so it covers add/remove/close and the glue lifecycle, not the per-frame drawing.
`test/windowtest.cpp` covers the collection and the handle's lifetime without a
backend at all. Run `testapp1` to exercise the rest — its extra windows are
app-owned, and their close button both animates and removes itself.

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

For cross-cutting rules (the `Any` convention, include-dir firewall, symbol
visibility) see `docs/conventions.md`; do not restate them here.
