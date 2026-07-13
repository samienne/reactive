# Architecture

*Last verified against `d2e8954` (2026-07-13).*

This is the mental model of the toolkit — how the pieces fit together and why.
It is deliberately signature-free; when you need exact types or arguments, read
the header it points at.

## The idea

The whole interface is a **composition of values that change over time**. You
never write code that mutates a widget in response to an event; instead you
describe how each piece of the UI is *derived* from application state, and the
framework re-evaluates and redraws the parts that actually changed. State enters
through a few well-defined points; everything downstream is pure derivation.

## Library stack

Dependencies flow one way: `btl → bq → ase → avg → bqui`.

| Library | Responsibility |
| --- | --- |
| `btl` | Foundational utilities (containers, cloning, function/tuple traits). |
| `bq` | The reactive core — signals and streams. Standalone; usable without any UI. |
| `ase` | GPU + platform abstraction (OpenGL, GLX/WGL windowing, a dummy backend). |
| `avg` | Vector graphics: paths, shapes, brushes/pens, text, the render tree, animation. |
| `bqui` | The UI toolkit built on all of the above. |

`bq` being independent of the UI is intentional — it is a general reactive
dataflow library that the UI happens to use.

## The reactive core (`bq`)

### Signals — values over time

A **signal** describes a value that varies over time; an `AnySignal<int>` is a
*description of how to compute an int*, not a stored int. Signals are stateless
graph descriptions — the actual current values and caches live in an evaluation
context, not in the signal object — which is what makes them cheap to copy and
safe to share. You do not read signals yourself; you hand them to the framework,
which evaluates them when it needs to draw.

State enters through **inputs** (`bq::signal::makeInput`), which hand back a
read-only signal plus a handle you push new values into. Signals are transformed
with `map` (derive a new signal) and combined with `merge`. A **constant** never
changes. See `src/bq/include/bq/signal/`.

Throughout the codebase, the `Any`-prefixed types (e.g. `AnySignal<T>`) are the
type-erased forms of the concrete `Signal<TStorage, Ts...>` — they drop the
internal storage type, keeping only the value type(s), and are what you store
and pass around. This `Any` = "type-erased" convention is used everywhere
(`AnyWidget`, `AnyShape`, …).

### Streams — events over time

A **stream** carries discrete events; unlike a signal (where only the latest
value matters) every value pushed into a stream is delivered. Streams model
user actions and other one-off events. Create one with `bq::stream::pipe`, push
through its handle, and fold events back into a signal with `iterate` — the main
bridge from events to state. See `src/bq/include/bq/stream/`.

## The UI model (`bqui`)

### Widgets are descriptions, realised by a pipeline

A widget is a *description*, not a live object. The user-facing type is the
type-erased `AnyWidget`. When the framework realises a widget it runs a small
pipeline, each stage lowering the description toward drawable state:

```
Widget  ──(BuildParams)──▶  Builder  ──▶  Element  ──▶  Instance
```

- **Widget** — the composable, user-facing description.
- **Builder** — receives build parameters (theme, environment) and produces size hints.
- **Element** — wires up the signals and receives the resolved size.
- **Instance** — the realised widget: its render tree, input areas, and hit regions, carried in a signal.

You mostly work at the Widget layer; the lower layers matter when writing custom
widgets that need their own size or environment access. See
`src/bqui/include/bqui/widget/` (`widget.h`, `builder.h`, `element.h`,
`instance.h`).

### Modifiers

A **modifier** wraps a widget with extra appearance or behaviour, applied with
the pipe operator and composed left to right:

```
widget::label("Hi") | modifier::margin(10.0f) | modifier::frame()
```

Because a modifier is just a function joined with `|`, anyone can write new ones
without touching the widget types — `AnyWidgetModifier` is itself type-erased.
Common ones live in `src/bqui/include/bqui/modifier/`.

### Layout

Layout is a multi-pass negotiation. Each widget exposes a **size hint** (a
minimum, a natural/maximum, and stretch capacity per axis); containers satisfy
minimums first, then distribute up to maximums, then hand leftover space to
fillers. Because width and height can be interdependent (e.g. text wrapping),
the negotiation runs in passes. **Gravity** aligns a widget within its allocated
space. Size hints are themselves signals, so layout re-runs when content
changes.

## Shapes

Shapes are a two-layer system:

- **`avg::ShapeFunction`** (the engine) — given `(time, DrawContext, size)` it
  produces an `avg::Shape` (geometry). It is wired into the animation system, so
  shape parameters can animate frame to frame. See `src/avg/include/avg/shapefunction.h`.
- **`bqui::shape::Shape<T>`** (the fluent builder) — wraps a signal of
  `ShapeFunction`. It is rvalue-chained: transforms (`translate`/`rotate`/
  `scale`/`transform`) and composition (`clip`, `strokeToShape`) return a new
  `Shape`; the terminal `fill`/`stroke`/`fillAndStroke` turn it into an
  `AnyWidget`. Concrete shapes today: `rectangle` (animatable corner radius),
  `circle`, `ellipse`. See `src/bqui/include/bqui/shape/`.

A shape's transforms are **paint-time** and never affect layout size; layout
sizing is a separate concern (see `docs/decisions.md`).

## Rendering (`avg` + `ase`)

Drawing is an **immutable render tree**. Nodes carry an `Animated<Obb>`
(oriented bounding box) and other animated properties. Updating produces a *new*
tree by splicing against the old one, which lets transitions animate between
states. `draw(time)` is a pure function of a timestamp, so the render thread can
redraw the current tree at any framerate while the signal graph independently
produces the next tree. Nodes report when they next need to redraw, so the
system is idle when nothing is animating.

Node kinds (see `src/avg/include/avg/rendertree.h`): `ContainerNode` (matches
children by id, diffing insert/remove/reorder), `ShapeNode`/`RectNode` (animated
leaves), `TransitionNode` (enter/leave animation), `ClipNode` (clips a child to
an OBB — **rectangle only** today), and `IdNode` (stable identity for matching).

`avg` builds the drawing; `ase` rasterises it through an OpenGL backend
(`glx` on Linux, `wgl` on Windows) or the headless `dummy` backend used for
tests.

## Animation

Animation is separate from the signal graph: signals define start/end states and
the animation system interpolates between them over time. `Animated<T>` carries
a value plus keyframes; `bqui::withAnimation` marks a scope (or a lambda) whose
signal changes should be animated with a given duration and curve. Curves live
in `avg::curve`. Infinite/oscillating animations are values that vary over time
without a state change. See `src/avg/include/avg/animated.h` and
`src/bqui/include/bqui/withanimation.h`.

## The environment (providers)

A typed, scoped key-value store is threaded through the widget tree so widgets
can read shared values (most commonly the `Theme`) without prop-drilling or
globals. A tag type names a parameter; `modifier::setParams`/`setTheme` push a
value for a subtree, and providers pull it back out as a signal argument to a
widget factory. See `src/bqui/include/bqui/provider/` and `modifier/setparams.h`.

## Concurrency

The design is task-based with no shared mutable state to protect: signals are
stateless graph descriptions (state lives in the evaluation context), the render
tree and drawings are immutable. That is what lets the render thread draw one
tree while the next is being produced.
