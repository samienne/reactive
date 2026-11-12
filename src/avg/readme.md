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

Text is drawn through a `Font` (`Drawing` also carries text entries). Layout
— extents, hit-testing — reads the font's unhinted, fractional metrics, so
text measures the same at every size and scale. Drawing it is separate:
ordinary filled, unrotated text is rasterised glyph by glyph into a
**glyph atlas** (one anti-aliased coverage texture per painter, so per window,
filled on demand and recycled least-recently-used) and drawn as textured
quads snapped to device pixels. Anything the atlas does not serve — stroked or
rotated text, very large sizes, a frame that overflows the atlas — falls back
to the glyph outlines, filled like any other shape.

## Ordering

A drawing paints its elements in order; there are no layers or z values. Two
additions shape what a frame can do beyond flat painting:

- **Pending shadows.** A surface's shadow belongs under the content of the
  container that holds the surface, not under the surface alone. A drawing
  therefore carries its shadows separately as *pending*; they travel with it
  through concatenation, clipping and transforms, and whoever renders the
  drawing first flushes them ahead of everything else.
- **Backdrop scopes.** A *scope* encloses part of a drawing and fixes what
  counts as lying behind it: the frame as it was when the scope was entered,
  never the scope's own content or its siblings. Effects that sample what
  lies behind them (frosted glass) see that backdrop. Whoever paints one
  child over another composes the two with `Drawing::over`, which scopes the
  upper child; a scope without effects only groups its content and costs
  nothing, so stacks use it unconditionally.
- **Effect elements.** A backdrop effect (`frostedGlass`, `tint`) is one
  element of the drawing, drawn through a shape: the effect's final pass
  paints the shape and the passes before it sample the scope's backdrop
  around it. The renderer does not draw effect elements yet; they take their
  place in the drawing so that the stacking and ordering above are already
  exercised.

In the render tree, a `SurfaceNode` (a filled shape under a child, with an
optional effect such as frosted glass behind the fill, animated like its
shadow) casts its shadow as a pending shadow and never opens a scope; a
`ContainerNode` built with `Stacking::stacked` is the stack that paints its
children over one another through `over`, while a flat container (vbox, hbox,
grid) only concatenates. `RenderTree::draw` is the root: it flushes the
drawing it returns.

## Animation

Time-varying values are `Animated<T>` — a value plus keyframes and a curve
(`avg::curve`). Anything that can be animated (a transform, a color, a corner
radius, a whole shape) is expressed this way, and interpolated by timestamp.

## Rendering a window

A window frame is never drawn onto the window directly. The painter draws it
into a full-window offscreen image of the window's pixel size (an sRGB colour
texture with a depth buffer, remade when the window is resized), then copies
that image onto the window with one textured quad. Every frame takes this one
path whether or not it uses effects, and the offscreen image is what later
backdrop effects such as frosted glass sample from.

## The render tree

Drawing is organised as an **immutable render tree** of nodes carrying animated
bounding boxes. Producing a frame splices a new tree against the old one (so
changes can animate), and drawing it is a pure function of a timestamp. This is
what lets rendering and state-updates run independently.

A tree can also describe itself rather than draw itself. A **snapshot** is a
machine-readable account of one tree at one instant — the node kinds, the
identities they carry, their boxes resolved into the space the snapshot was
taken in, and the text the leaves draw. It is a plain data structure; turning it
into a JSON document for a reader outside the process is `bqui`'s job, not
`avg`'s. It is taken from the same immutable tree a frame is drawn from, so it
can never describe a half-updated screen.

## Layout

- `path.h`, `pathbuilder.h`, `shape.h` — geometry.
- `brush.h`, `pen.h`, `color.h`, `transform.h` — styling and transforms.
- `drawing.h` — the rasterisable output; `font.h`, `fontmanager.h` — text.
- `animated.h`, `curve/` — animation.
- `rendertree.h` — the scene graph.

For the internal models (render tree lifecycle, the shape/region pipeline,
animation traits) and the traps to know before editing, see `AGENTS.md` in this
directory.
