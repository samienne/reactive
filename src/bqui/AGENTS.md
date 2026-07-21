# bqui — agent notes

*Last verified against `916c25b` (2026-07-21).*

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

**The window list is the only thing that says which windows exist.** `App` holds
it as a `bq::signal::ArraySignal<Window>` and `App::run` maps it to one
`WindowGlue` per identity and joins the glues, so the array itself creates a
glue when an identity appears and destroys it — closing the window — when the
identity leaves; a surviving window is never rebuilt. A `WindowGlue` owns
everything for one window: its `ase::Window`, painter, per-window signal
contexts, and input state. The glues are re-read from the joined signal only on
the frames in which membership changed, and `AnimationGuard` walks that cached
vector.

`App` therefore reports nothing about the open windows and has no hook for
observing them. A window closes by leaving the list, so what a window offers —
`Window::onClose`, a button in its own UI — is wired to whatever the list is
built from, and the removal flows back through the array. `src/testapp1` is the
worked example: its second window's title bar and its own button both take the
same key out of the same input.

Two ordering rules hold that together, and both are load-bearing rather than
tidiness:

- **A window's signals are updated in the frame phase, never from an input
  handler.** The handler marks the window for redraw and returns; the platform
  runs the frame after `App::run`'s callback has reconciled the list, so a
  window whose key has just left is destroyed before anything updates it. A
  handler that transacted inline updated a signal naming a key it had itself
  removed, and that signal throws once the key is gone — a window with its own
  close button took the process down that way.
- **A glue is released only after the main render queue has caught up.** An
  in-flight frame holds the window's framebuffer, which holds the window by
  reference.

The close path itself never transacts: `WindowGlue`'s close callback only
invokes the window's own callbacks, so removing a window from its `onClose` is
safe by construction rather than by the rule above. That is worth knowing
because it is what makes the idiom safe.

Two paths still break the first rule and are **known gaps**, not oversights to
re-derive:

- `AnimationGuard` transacts every glue from its constructor and destructor, and
  `withAnimation` is called from input handlers. A handler that both animates
  and removes a window still throws, for the window that left. Reaching it takes
  an explicit `withAnimation` scope around the removal; the close path does not
  open one. Fixing it means the animation bracket no longer being a pair of
  synchronous transactions.
- A window list changed from *within* the frame phase — one window's update
  removing another's key — is reconciled only on the next pass, so the other
  window can update once with its key already gone.

Neither is reachable from `test/apptest.cpp`: the dummy backend never runs a
window's frame callback at all, so that binary covers the array plumbing and
nothing about this ordering. Run `testapp1` to exercise it.

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
