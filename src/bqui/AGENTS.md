# bqui — agent notes

*Last verified against `221cd3f` (2026-07-19).*

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

`App` holds its windows as a reactive `WindowList` — a signal of `(stable-id,
Window)` pairs, the same shape `dataBind` produces for widgets. `windows({...})`
is a constant of that shape; `windows(signal)` and `onWindowsReconciled` take
the dynamic path. `App::run` samples the list once per frame (a dedicated
`SignalContext`, before the running signal) and reconciles `WindowGlue`s keyed
by id: a new id constructs a glue (which creates its `ase::Window` on demand), a
vanished id drops it (closing the window), existing ones persist. A `WindowGlue`
owns everything for one window (its `ase::Window`, painter, per-window signal
contexts, input state).

## Traps

- The exported template instantiations for `Widget`/`Element`/modifiers live in
  `src/bqui/src/widget/widget.cpp`, export-marked for clang-cl (see
  `docs/conventions.md`).
- Shape builder methods are `&&`-qualified — call other `&&` overloads via
  `std::move(*this)`. A parameterless shape can't use `merge()`; it's built as a
  constant (the `if constexpr (sizeof...(Ts) == 0)` branch in `shape/shape.h`).
- Builder-style template APIs are guarded by an instantiation smoke test
  (`test/shapetest.cpp`) — extend it when adding builder methods.

For cross-cutting rules (the `Any` convention, include-dir firewall, symbol
visibility) see `docs/conventions.md`; do not restate them here.
