# avg — agent notes

*Last verified against `44d4091` (2026-07-22); the text sections against
`f3ed6e5` (2026-10-22).*

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

A `Snapshot` is pure data — node kinds, ids, resolved boxes and the text the
leaves draw. `avg` deliberately does not serialise it: JSON is a `bqui` concern
(it owns the `nlohmann` dependency and the remote wire format), so the schema
lives in `bqui`'s remote layer, not here. Keep `Snapshot` free of any
serialisation coupling.

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

Text goes through `Font`/`FontManager` (`font.h`, `fontmanager.h`); FreeType
is the backend. Layout shapes text into a `Path` out of a caller-supplied
`pmr::memory_resource` — the same resource a `DrawContext` carries. Drawing
goes through a glyph atlas instead, and falls back to that path. Entry
points: `fontmanager.cpp` (`walkGlyphs`, `FontManager::rasterizeGlyph`, to
which `Font::rasterizeGlyph` forwards),
`src/atlasallocator.h` (`AtlasAllocator`, the packing), `src/glyphatlas.h`
(`GlyphAtlas`, the texture), `rendering.cpp` (`generateTextQuads`, the draw
path and its fallback rules) and `painter.cpp` (the text pipeline and its
shader).

- One walk over the text (`walkGlyphs` in `fontmanager.cpp`) drives the path,
  the extents, hit-testing and `Font::getGlyphs`; change layout there, not in
  the callers.
- Metrics and outlines are read at one fixed FreeType size (the "metric size",
  whose 26.6 coordinates are divided by 6400 to get the unscaled units).
  `Font::rasterizeGlyph` uses separate per-scale `FT_Size` objects kept on the
  `FontImpl`, and must leave the metric size active again before anything else
  touches the face — the glyph cache assumes it.
- **Nothing is hinted** (`FT_LOAD_NO_HINTING` for metrics, outlines and
  bitmaps alike), so layout metrics stay fractional and the atlas path and
  the outline path measure identically. `GlyphKey` (`atlasallocator.h`) is a
  struct of `font`, `glyphIndex`, `pixelSize` and `subpixelBin` rather than a
  packed integer so a hinting mode can be added as one more field without
  touching the allocator or the atlas.
- `Font::rasterizeGlyph` renders one glyph at an integer pixel size with its
  outline shifted by one of `subpixelBins` (4) fractions of a pixel in x.
  There are no y bins: `generateTextQuads` rounds the pen's y to the device
  pixel.
- **Fallback rules** live in `generateTextQuads`. A `TextEntry` is drawn as
  atlas quads only when it has a brush and no pen, neither it nor any
  enclosing transform rotates (which also covers a rotated clip, since the
  clip rectangle is taken in the enclosing space), and its rounded device
  pixel size is within 1..`maxAtlasPixelSize` (128). Any other entry, and an
  entry whose glyph the atlas cannot place this frame, goes through
  `textToPath` and the outline tessellation like a shape; nothing is ever
  rasterised through a transform. Each quad is clipped against the enclosing
  clip rectangle in device space.
- The text pipeline (`painter.cpp`) samples the atlas as coverage and raises
  it to `1 / textGamma` (1.45) before it scales the premultiplied colour.
  The value is tuned by eye; see `docs/decisions.md`.

### Glyph atlas allocator

`src/atlasallocator.h` is a private header (the test binary adds `src` to its
include path for it): `AtlasAllocator` tracks which `GlyphKey` occupies which
pixel rectangle of an atlas texture of a given size, with no GPU, FreeType or
`ase` dependency. The caller uploads pixels and grows the atlas by replacing
the allocator.

- **Packing is shelf-based.** Shelves are cut top-down from the unused strip
  at the bottom; a glyph goes on a shelf whose height is its own, rounded up
  to a multiple of 4 px, plus 2 px of padding (so 13 px and 16 px glyphs share
  an 18 px shelf). Each shelf keeps a sorted free list of x-ranges; an insert
  takes the best-fit range on a matching shelf, else cuts a new shelf.
- **Padding is inside the allocation.** Every allocation is 2 px wider than
  the glyph and spans the whole shelf height; the returned `AtlasRect` is the
  1 px inset at the requested size. `GlyphAtlas` uploads the padded
  (w + 2) x (h + 2) block around it with a zero border, so a slot never
  keeps a previous occupant's texels at its edges.
- **Eviction is LRU, bounded by frames.** Entries sit in an index-linked list
  over a vector (no per-insert vector growth once warm); `find`/`insert` move an
  entry to the front and stamp it with the current frame, and an entry stamped
  with the current frame is never evicted. Because touching moves to the front,
  a pinned tail means everything is pinned, which is the only case `insert`
  fails.
- **Only trailing shelves are released.** An evicted range is coalesced back
  into its shelf's free list. A shelf that becomes entirely free is returned to
  the unused strip only while it is the one adjacent to the strip (the while
  loop then takes any free shelves behind it); an entirely free *interior*
  shelf stays a shelf of its height and is reused by glyphs of that height.
  This keeps shelf indices stable and the release logic trivial; the cost is
  that a run of empty interior shelves cannot be re-cut at a new height until
  everything below them has also gone. Relayout on growth, not here.

### Glyph atlas

`src/glyphatlas.h` (private, like the allocator) puts an `AtlasAllocator`
behind a `FORMAT_R8` texture of its `ase::RenderContext`: `get` rasterises a
missing glyph through `Font::rasterizeGlyph`, places it, and queues the
bitmap; `endFrame` pushes the queued `pushSubUpload`s into the command buffer
the caller then pushes the frame's draws into. The atlas starts at 512 px
square and doubles up to 2048.

- **Uploads stay in the draw buffer.** `render()` (`rendering.cpp`) calls
  `endFrame` on the very `CommandBuffer` it then pushes the draws into, ahead
  of them. Several `render()` calls accumulate into one buffer before it is
  submitted, and the GL queue sorts only the runs of draw commands between
  non-draw commands, so an in-line upload is a fence: a later `render()` into
  the same buffer cannot overwrite texels an earlier one's draws still sample.
  Routing the uploads through a separate buffer would lose that ordering.
- **Overflow is deferred to the frame end.** Quads already emitted reference
  the current texels, so a failed insert only flags the atlas: every further
  miss that frame returns nullopt (hits still serve), and `endFrame` then
  either doubles the atlas (new allocator *and* new texture, everything
  forgotten) or, at the maximum size, clears it. The uploads queued before
  the overflow still go out, to the texture the draws used.
- **Slot metadata outlives the allocator's entries.** Bearings live in a
  `GlyphKey -> GlyphSlot` map beside the allocator, which evicts silently, so
  a hit is confirmed with `AtlasAllocator::find` and stale entries are swept
  once the map reaches twice the live count plus 64 entries. Blank glyphs (a
  space) are in
  that map only, never in the allocator or the texture.
- **The atlas keeps its fonts alive.** `GlyphKey::font` is the address
  `Font::getIdentity` returns, and `FontManager` caches `FontImpl` by weak
  pointer, so a dropped font's address could be reused; the atlas holds a
  `Font` per identity it has keyed until it next clears.

For cross-cutting rules (symbol visibility, the include-dir firewall) see
`docs/conventions.md`; do not restate them here.
