# avg

`avg` is a C++17 **vector graphics** library: geometry, styling, text, and an
animated, immutable render tree. It is the drawing layer beneath the UI toolkit
(`bqui`) and is rasterised by `ase`.

## Geometry

A **`Shape`** is resolution-independent geometry, built from a `Path`
(`PathBuilder` assembles line/curve/arc segments). Shapes support boolean
composition (intersect, add, subtract) and affine transforms, so complex outlines
are composed rather than drawn imperatively.

## Styling

Geometry becomes a **`Drawing`** — the flat, immutable description that gets
rasterised — by styling it:

- a **`Brush`** (from a `Color`) fills a shape,
- a **`Pen`** strokes its outline.

Text is drawn through a `Font` (`Drawing` also carries text entries).

## Animation

Time-varying values are `Animated<T>` — a value plus keyframes and a curve
(`avg::curve`). Anything that can be animated (a transform, a color, a corner
radius, a whole shape) is expressed this way, and interpolated by timestamp.

## The render tree

Drawing is organised as an **immutable render tree** of nodes carrying animated
bounding boxes. Producing a frame splices a new tree against the old one (so
changes can animate), and drawing it is a pure function of a timestamp. This is
what lets rendering and state-updates run independently.

A tree can also describe itself rather than draw itself. A **snapshot** is a
machine-readable account of one tree at one instant — the node kinds, the
identities they carry, their boxes resolved into the space the snapshot was
taken in, and the text the leaves draw — serialised as a versioned JSON
document for a reader outside the process. It is taken from the same immutable
tree a frame is drawn from, so it can never describe a half-updated screen.

## Layout

- `path.h`, `pathbuilder.h`, `shape.h` — geometry.
- `brush.h`, `pen.h`, `color.h`, `transform.h` — styling and transforms.
- `drawing.h` — the rasterisable output; `font.h`, `fontmanager.h` — text.
- `animated.h`, `curve/` — animation.
- `rendertree.h` — the scene graph.

For the internal models (render tree lifecycle, the shape/region pipeline,
animation traits) and the traps to know before editing, see `AGENTS.md` in this
directory.
