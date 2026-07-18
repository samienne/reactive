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

**A window list is the only thing that says which windows exist.** There are two
of them and they do not overlap:

- **The app's own collection** (`src/windowlist.h`), a
  `bq::signal::SharedVector<Window>` keyed into an array by
  `bq::signal::forEach` with `Window::getId` as the key and the identity
  delegate, so the elements are `AnySignal<Window>`. `App::addWindows`
  appends to it, `App::removeWindow` and `Window::close` remove from it, and
  `App::getWindows` / `App::getWindowsSignal` read it — as a snapshot and as a
  signal respectively.
- **Caller-supplied arrays**, added with `App::addWindowArray`. The caller's own
  model is the source of truth for those, so nothing in `App` mutates them and
  `close()` does not reach them.

`App::run` concatenates the two, maps the result to one `WindowGlue` per
identity and joins the glues, so the array itself creates a glue when an
identity appears and destroys it — closing the window — when the identity
leaves; a surviving window is never rebuilt. A `WindowGlue` owns everything for
one window: its `ase::Window`, painter, per-window signal contexts, and input
state. The glues are re-read from the joined signal only on the frames in which
membership changed, and `AnimationGuard` walks that cached vector. The no-signal
`run()` overload stops when that cached vector is empty, which is why it counts
both kinds of window.

The concatenation happens once, at the start. The app's own collection is a live
signal and so follows additions while the app runs; a *further array* added
after `run` started could not be seen, so `addWindowArray` rejects it rather
than opening nothing. Picking one up would need the reactive subtree
(`AnySignal<ArraySignal<T>>`) that `docs/design/arraysignal.md` still lists as
open.

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
  runs the frame after `App::run`'s callback has reconciled the list, so a
  window whose key has just left is destroyed before anything updates it.
- **A glue is released only after the main render queue has caught up.** An
  in-flight frame holds the window's framebuffer, which holds the window by
  reference.

Removal itself never evaluates a signal on either path. `WindowGlue`'s close
callback invokes the window's own callbacks and then removes it, and a removal
writes the `SharedVector`, whose publication only sets an input.

**A glue reads its window's signal once and never updates it.**
`WindowGlue::windowSignal_` is a `SignalContext` over the element signal,
evaluated in the constructor to produce `window_` and left alone thereafter.
There is nothing later to read — an element cannot vary at a fixed identity — so
the read that would encounter a departed key never happens, and everything the
glue drives per frame is the window's *own* title and widget signals, which came
out of that one read.

Do not be tempted to update it. That is the consumer-with-its-own-clock case the
design forbids, and it is the one thing that would put the departed-key throw
back on this path.

Both kinds of window reach the glue the same way: a caller-supplied
`ArraySignal<Window>` is lifted to `ArraySignal<AnySignal<Window>>` with a
`map` to `constant`, so `App::run` joins one array and not two shapes. What
differs is what is *inside* the window: a caller's window built by a
three-argument `forEach` delegate may hold a pick in its title or widgets, and
those the glue does drive per frame. The gaps below are gaps in that path only.

- `AnimationGuard` transacts every glue from its constructor and destructor, and
  `withAnimation` is called from input handlers. A handler that both animates
  and removes a window transacts the departing window synchronously. For an
  app-owned window that is harmless, by the paragraph above; for one from a
  caller-supplied array it still throws.
- A window list changed from *within* the frame phase — one window's update
  removing another's key — is reconciled only on the next pass, so the other
  window can update once with its key already gone. Same restriction: it is the
  pick that throws.

None of this is reachable from `test/apptest.cpp`: the dummy backend never runs
a window's frame callback at all, so that binary covers the array plumbing and
nothing about the ordering. `test/windowtest.cpp` covers the collection and the
handle's lifetime without a backend at all. Run `testapp1` to exercise the rest
— its extra windows are app-owned, and their close button both animates and
removes itself.

## Agent layer

`agent/` is the headless agent-control surface, independent of the widget
pipeline:

- `introspectionjson.h` — `toJson` serialises a resolved `Introspection` tree
  (absolute window-space obbs) to JSON: the observe payload.
- `transport.h` — a swappable, length-prefixed framed message channel
  (`connect`/`listen`+`accept`); the local IPC is a named pipe on Windows and a
  Unix-domain socket elsewhere. It carries bytes only, no protocol knowledge.

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
