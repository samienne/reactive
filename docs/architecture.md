# Architecture

*Last verified against `d2e8954` (2026-07-13); the rendering paragraph against
`f541ac65` (2026-11-12).*

The cross-cutting overview: how the libraries fit together and the ideas that
span them. Per-library depth lives in each library's own docs (indexed below);
this file stays an overview.

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
| `btl` | Foundational utilities (containers, cloning, tuple/function traits, async). |
| `bq` | The reactive core — signals and streams. Standalone; no UI dependency. |
| `ase` | GPU + platform abstraction (OpenGL via GLX/WGL, a headless backend). |
| `avg` | Vector graphics: paths, shapes, styling, text, the render tree, animation. |
| `bqui` | The UI toolkit built on all of the above. |

## How it fits together

An application describes its UI as `bqui` widgets whose content is derived from
`bq` **signals** (state) and fed by `bq` **streams** (events). When the framework
realises those widgets it lowers them through a pipeline (`Widget → Builder →
Element → Instance`) into an **`avg` render tree** — an immutable, animated scene
graph. `ase` rasterises the resulting drawing through OpenGL.

A drawing is a flat list painted in order. A window frame is drawn into one
full-window offscreen image and copied onto the window with a single quad, so
the frame exists as a texture while it is being painted. That is what lets a
**backdrop effect** (frosted glass behind a card) show what lies below it: a
container that paints one child over another wraps the upper child in a
*backdrop scope*; an effect inside the scope reads a snapshot of the image as
it stood when the scope was entered, runs a few shader passes over that
snapshot, and paints the result through its shape in its own place in the
paint order. Nothing is analysed per element, and a scope with no effect in
it costs nothing. The design is
[`design/backdrop-effects.md`](design/backdrop-effects.md).

Because signals are stateless graph descriptions (their state lives in an
evaluation context), and the render tree and drawings are immutable, there is no
shared mutable state to guard: the render thread can draw the current tree at any
framerate while the signal graph independently produces the next one. The system
is idle when nothing is animating.

That is the whole loop — **state in, derivation out, redraw only what changed.**
How each stage works in detail is documented with the library that owns it.

## Per-library docs

Each library is self-describing. Read the concepts in its `readme.md`, and the
internals/traps in its `AGENTS.md`, when working in that library:

- `bq` — signals & streams: [readme](../src/bq/readme.md) · [agent notes](../src/bq/AGENTS.md)
- `avg` — vector graphics & render tree: [readme](../src/avg/readme.md) · [agent notes](../src/avg/AGENTS.md)
- `bqui` — the UI toolkit: [readme](../src/bqui/readme.md) · [agent notes](../src/bqui/AGENTS.md)
- `ase` — GPU & platform: [readme](../src/ase/readme.md) · [agent notes](../src/ase/AGENTS.md)
- `btl` — utilities: [readme](../src/btl/readme.md) · [agent notes](../src/btl/AGENTS.md)

Cross-cutting rules and traps that are not owned by a single library live in
`conventions.md`; the reasoning behind non-obvious choices is in `decisions.md`.
