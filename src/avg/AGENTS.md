# avg — agent notes

*Last verified against `44d4091` (2026-07-22).*

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
- `DrawContext` (`drawcontext.h`) is the memory a draw allocates out of, and
  nothing else. It takes either a `Painter*` or a bare `pmr::memory_resource*`,
  so a tree can be drawn and its `Drawing` asserted with no rendering back end.
- `Drawing` (`drawing.h`) is the flat rasterisable output — a variant of
  `ShapeElement` / `TextEntry` / `ClipElement` / `RegionFill`. **Clipping is
  rectangular only**: `Drawing::clip` takes `Rect`/`Obb`, and there is no
  region/shape clip. (Path-level clipping is deferred — `docs/decisions.md`.)
- **Trap:** `Shape::getControlBb` applies a composed element's transform twice
  in its `Operation` branch; the sibling `StrokeToShape` branch is correct.
  Known and unfixed — geometry derived from it inherits the error.

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

## Render tree snapshots

`rendertree/snapshot.h` describes a tree instead of drawing it, for tooling
outside the process. `RenderTree::snapshot` mirrors `draw`: same arguments,
same pure-function contract, same subtree — a transition contributes only the
branch that is on screen. Each node describes itself through the `snapshot`
virtual, so adding a node kind means adding that override.

- **Geometry is resolved, not authored.** A node reports its box already
  composed with every enclosing obb, so a reader places it without a transform
  of its own. It is derived from the nodes' `Animated<Obb>`, never from
  `Shape::getControlBb`, so it does not inherit that function's trap above.
- **Text comes from drawing, not from the tree.** A leaf's content lives inside
  its draw function, so a leaf is drawn on its own out of the caller's memory
  and the `TextEntry`s of the result are collected; the drawing is discarded.
  A snapshot therefore costs a whole draw pass. Because a leaf is drawn in
  isolation, an enclosing `ClipNode` applies itself afterwards, through
  `clipSnapshotText`.
- **Nulls are expected.** An empty tree yields no root, and a node whose child
  is null yields no child; the walker never assumes a well-formed tree.

`toJson` writes schema version 1: an envelope of `version`, `time`, `obb` and
`root`, and per node `type`, `obb` (`center`/`size`/`angle`), `text` and
`children`, plus `id` when the node has one and `leaving` when the subtree is
on its way out. Text is passed through as the UTF-8 bytes the `TextEntry`
carries. The schema is a contract — extend it additively and bump
`Snapshot::version` when an existing field changes meaning.

## Animation

- `Animated<T>` (`animated.h`): a value plus keyframes; evaluated by timestamp
  via `AnimatedTraits<T>`. `getValue(time)`, `updated(...)`, `hasAnimationEnded`.
- The `Animated<T>` explicit template instantiations live in
  `src/avg/src/animated.cpp` and are export-marked — if you add an animated type,
  add it there too (see `docs/conventions.md` on template export vs clang-cl).

## Vendored Clipper

`src/clipper/` is upstream Clipper, not ours. It builds as its own `libclipper`
static target with warnings off, so a file added there gets no diagnostics —
and a file added to `libavgsrc` instead gets all of them. Keep changes to it to
what upstream would recognise.

## Text

Text goes through `Font`/`FontManager` (`font.h`, `fontmanager.h`), which shape
text into a `Path` out of a caller-supplied `pmr::memory_resource` — the same
resource a `DrawContext` carries. FreeType is the backend.

For cross-cutting rules (symbol visibility, the include-dir firewall) see
`docs/conventions.md`; do not restate them here.
