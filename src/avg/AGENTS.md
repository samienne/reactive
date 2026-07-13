# avg — agent notes

*Last verified against `d2e8954` (2026-07-13).*

Internals, entry points, and traps for the vector-graphics layer. Concepts and
usage are in `readme.md`; project-wide conventions are in the top-level `docs/`.
This file is the index for `avg`; split topics into a `.agent/` folder here if it
outgrows one file.

## Shapes and drawings

- `Shape` (`shape.h`) is geometry: a tree of `Path`/boolean-op/stroke elements
  with per-element transforms. Boolean ops are `intersect`/`add`/`subtract`;
  there is **no `operator&`** (use `intersect`). It can rasterise to a `Region`
  via `fillToRegion`/`strokeToRegion`.
- `ShapeFunction` (`shapefunction.h`) is a time-parameterised shape:
  `(time, DrawContext, size) -> Shape`, wired into the animation system. This is
  what `bqui`'s shape builder wraps.
- `Drawing` (`drawing.h`) is the flat rasterisable output — a variant of
  `ShapeElement` / `TextEntry` / `ClipElement` / `RegionFill`. **Clipping is
  rectangular only**: `Drawing::clip` takes `Rect`/`Obb`, and there is no
  region/shape clip. (Path-level clipping is deferred — `docs/decisions.md`.)

## Render tree

- `rendertree.h`: an **immutable** tree of `RenderTreeNode`s. Node kinds:
  `ContainerNode` (matches children by id; diffs insert/remove/reorder),
  `ShapeNode`/`RectNode` (animated leaves), `TransitionNode` (enter/leave),
  `ClipNode` (clips a child to an `Animated<Obb>` — **rectangle only**),
  `IdNode` (stable identity for matching).
- `oldTree.update(newTree, options, time)` splices trees (creating animated
  transitions); `draw(context, obb, time)` is a **pure** function of the
  timestamp and returns whether animation continues, so nodes report when they
  next need a redraw.

## Animation

- `Animated<T>` (`animated.h`): a value plus keyframes; evaluated by timestamp
  via `AnimatedTraits<T>`. `getValue(time)`, `updated(...)`, `hasAnimationEnded`.
- The `Animated<T>` explicit template instantiations live in
  `src/avg/src/animated.cpp` and are export-marked — if you add an animated type,
  add it there too (see `docs/conventions.md` on template export vs clang-cl).

## Text

Text goes through `Font`/`FontManager` (`font.h`, `fontmanager.h`); `DrawContext`
exposes both the path builder and text shaping. FreeType is the backend.

For cross-cutting rules (symbol visibility, the include-dir firewall) see
`docs/conventions.md`; do not restate them here.
