#include "rendering.h"

#include "traceresource.h"
#include "debug.h"
#include "glyphatlas.h"

#include <avg/softmesh.h>
#include <avg/effect.h>
#include <avg/effectemitter.h>
#include <avg/effectplan.h>
#include <avg/obb.h>
#include <avg/region.h>
#include <avg/jointype.h>
#include <avg/endtype.h>
#include <avg/region.h>
#include <avg/positionedglyph.h>

#include <ase/uniformbufferrange.h>
#include <ase/uniformset.h>
#include <ase/usage.h>
#include <ase/buffer.h>
#include <ase/matrix.h>
#include <ase/pipeline.h>
#include <ase/rendercontext.h>
#include <ase/texture.h>
#include <ase/async.h>

#include <btl/fnv1a.h>
#include <btl/uhash.h>
#include <btl/hash.h>

#include <pmr/vector.h>
#include <pmr/unsynchronized_pool_resource.h>
#include <pmr/monotonic_buffer_resource.h>
#include <pmr/new_delete_resource.h>

#include <tracy/Tracy.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <variant>

namespace avg
{

namespace
{

template <class... Ts>
struct Overloaded : Ts...
{
    using Ts::operator()...;
};

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

// Vertex: rgbaxyz
constexpr size_t vertexFloats = 7;
constexpr size_t vertexZ = 6;
// Text vertex: rgbaxyzuv
constexpr size_t textVertexFloats = 9;

constexpr float maxUnrotated = 0.0001f;
constexpr int maxAtlasPixelSize = 128;

struct Element
{
    ase::Pipeline pipeline;
    std::vector<ase::Texture> textures;
    size_t floatsPerVertex;
    pmr::vector<float> vertices;
    pmr::vector<uint32_t> indices;
};

// The glyph quads of one TextEntry, in the root space of the drawing and
// with their z still to be set.
struct TextQuads
{
    pmr::vector<float> vertices;
    pmr::vector<uint32_t> indices;
};

// An effect element in device pixels: the regions its passes write and its
// shape as a triangle list, reading the snapshot of scope.
struct EffectDraw
{
    Effect const* effect;
    size_t scope;
    EffectRegions regions;
    std::vector<Vector2f> triangles;
};

// Where a backdrop scope is entered in paint order.
struct ScopeEntry
{
    size_t scope;
};

using Item = std::variant<SoftMesh, TextQuads, EffectDraw, ScopeEntry>;

struct TextContext
{
    GlyphAtlas& atlas;
    float scalingFactor;
    ase::Texture texture;
    ase::Vector2i textureSize;
};

// A backdrop scope of the frame: used once an effect directly inside it
// reads its snapshot, whose region is the union of those reads.
struct Scope
{
    PixelRect backdropRegion;
    bool used = false;
    size_t snapshot = 0;
};

struct EffectContext
{
    float scalingFactor;
    Vector2i screenSize;
    std::vector<Scope> scopes;
};

bool isMesh(Item const& item)
{
    return std::holds_alternative<SoftMesh>(item)
        || std::holds_alternative<TextQuads>(item);
}

PixelRect toDevicePixels(Rect const& r, float scalingFactor)
{
    // Larger than any target and exact in a float, so the casts and the
    // differences below cannot overflow.
    float const limit = 16777216.0f;
    auto toPixel = [limit](float v)
    {
        return (int)std::clamp(v, -limit, limit);
    };

    int const left = toPixel(std::floor(r.getLeft() * scalingFactor));
    int const bottom = toPixel(std::floor(r.getBottom() * scalingFactor));
    int const right = toPixel(std::ceil(r.getRight() * scalingFactor));
    int const top = toPixel(std::ceil(r.getTop() * scalingFactor));

    return PixelRect{ left, bottom, right - left, top - bottom };
}

// The filled shape as a triangle list in device pixels; toRoot maps the
// shape's space to the root of the drawing.
std::vector<Vector2f> triangulateToDevice(pmr::memory_resource* memory,
        Shape const& shape, Transform const& toRoot, Vector2f pointSize,
        float scalingFactor)
{
    ZoneScoped;
    pmr::monotonic_buffer_resource mono(memory);
    auto [vertices, indices] = shape.fillToRegion(&mono, pointSize)
        .triangulate(&mono);

    std::vector<Vector2f> result;
    result.reserve(indices.size());
    for (auto index : indices)
        result.push_back(Vector2f((toRoot * vertices[index]) * scalingFactor));

    return result;
}

Color premultiply(Color c)
{
    return Color(
            c.getAlpha() * c.getRed(),
            c.getAlpha() * c.getGreen(),
            c.getAlpha() * c.getBlue(),
            c.getAlpha()
            );
}

pmr::vector<float> generateVertices(pmr::memory_resource* memory,
        SoftMesh const& mesh, float z)
{
    ZoneScoped;
    auto const& vertices = mesh.getVertices();
    Transform const& t = mesh.getTransform();
    std::array<float, 4> c = premultiply(mesh.getBrush().getColor())
        .getArray();

    Matrix2f rs = t.getRsMatrix();

    pmr::vector<float> result(memory);
    result.reserve(vertices.size() * vertexFloats);
    for (auto&& v : vertices)
    {
        Vector2f p = t.getTranslation() + rs * Vector2f(v[0], v[1]);

        result.insert(result.end(), { c[0], c[1], c[2], c[3], p[0], p[1], z });
    }

    return result;
}

SoftMesh generateMeshForRegion(pmr::memory_resource* memory,
        Region const& region, Brush const& brush,
        Rect const& r, bool clip)
{
    ZoneScoped;
    auto bufs = std::make_pair(
            pmr::vector<ase::Vector2f>(memory),
            pmr::vector<uint32_t>(memory)
            );

    if (clip)
        bufs = region.getClipped(r).triangulate(memory);
    else
        bufs = region.triangulate(memory);

    Color color = premultiply(brush.getColor());

    return SoftMesh(std::move(bufs.first), std::move(bufs.second), brush);
}

SoftMesh generateMeshForBrush(pmr::memory_resource* memory,
        Shape const& shape, Brush const& brush,
        ase::Vector2f pointSize, Rect const& r, bool clip)
{
    ZoneScoped;
    pmr::monotonic_buffer_resource mono(memory);

    Region region = shape.fillToRegion(&mono, pointSize);

    return generateMeshForRegion(memory, region, brush, r, clip);
}

SoftMesh generateMeshForPen(pmr::memory_resource* memory, Shape const& shape,
        Pen const& pen, ase::Vector2f pointSize, Rect const& r, bool clip)
{
    ZoneScoped;
    pmr::monotonic_buffer_resource mono(memory);

    Region region = shape.strokeToRegion(&mono, pen, pointSize);

    return generateMeshForRegion(memory, region, pen.getBrush(), r, clip);
}

pmr::vector<Item> generateMeshesForShapeElement(
        pmr::memory_resource* memory, Drawing::ShapeElement const& element,
        ase::Vector2f pointSize, Rect const& r, bool clip)
{
    ZoneScoped;
    auto const& shape = element.shape;
    auto const& brush = element.brush;
    auto const& pen = element.pen;

    pmr::vector<Item> result(memory);

    if (brush.has_value())
    {
        bool needClip = !shape.getControlBb().isFullyContainedIn(r);
        result.push_back(generateMeshForBrush(memory, shape, *brush, pointSize,
                    r, clip && needClip));
    }

    if (pen.has_value())
    {
        Rect penRect = shape.getControlBb().enlarged(pen->getWidth());
        bool needClip = !penRect.isFullyContainedIn(r);

        result.push_back(generateMeshForPen(memory, shape, *pen, pointSize,
                    r, clip && needClip));
    }

    return result;
}

bool isUnrotated(Transform const& t)
{
    return std::abs(t.getRotation()) < maxUnrotated;
}

struct DeviceQuad
{
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
};

// The quad's left edge is at x0, its right at x1, its bottom at y0 and its
// top at y1; u0/v0 belong to the top-left corner and u1/v1 to the
// bottom-right one, as the atlas rows run from the top.
std::optional<DeviceQuad> clipQuad(DeviceQuad q, Rect const& clip)
{
    float const cx0 = std::max(q.x0, clip.getLeft());
    float const cx1 = std::min(q.x1, clip.getRight());
    float const cy0 = std::max(q.y0, clip.getBottom());
    float const cy1 = std::min(q.y1, clip.getTop());

    if (cx0 >= cx1 || cy0 >= cy1)
        return std::nullopt;

    float const du = (q.u1 - q.u0) / (q.x1 - q.x0);
    float const dv = (q.v1 - q.v0) / (q.y1 - q.y0);

    DeviceQuad result;
    result.x0 = cx0;
    result.x1 = cx1;
    result.y0 = cy0;
    result.y1 = cy1;
    result.u0 = q.u0 + (cx0 - q.x0) * du;
    result.u1 = q.u1 - (q.x1 - cx1) * du;
    result.v0 = q.v0 + (q.y1 - cy1) * dv;
    result.v1 = q.v1 - (cy0 - q.y0) * dv;

    return result;
}

// The glyph quads of textEntry, or nullopt when it has to be drawn as
// outlines: it is stroked, rotated, larger than the atlas serves, or the
// atlas has no room for one of its glyphs this frame. toRoot maps the
// entry's enclosing space to the root of the drawing; clipRect is in that
// enclosing space.
std::optional<TextQuads> generateTextQuads(
        pmr::memory_resource* memory,
        TextContext& text,
        Transform const& toRoot,
        TextEntry const& textEntry,
        std::optional<Rect> const& clipRect)
{
    ZoneScopedN("Text quads");

    if (!textEntry.getBrush().has_value() || textEntry.getPen().has_value())
        return std::nullopt;

    if (!isUnrotated(toRoot) || !isUnrotated(textEntry.getTransform()))
        return std::nullopt;

    float const sf = text.scalingFactor;
    Transform const toDevice = toRoot * textEntry.getTransform();
    int const pixelSize = static_cast<int>(
            std::lround(toDevice.getScale() * sf));

    if (pixelSize < 1 || pixelSize > maxAtlasPixelSize)
        return std::nullopt;

    std::optional<Rect> deviceClip;
    if (clipRect.has_value())
    {
        deviceClip = clipRect->scaled(toRoot.getScale())
            .translated(toRoot.getTranslation())
            .scaled(sf);
    }

    Font const& font = textEntry.getFont();
    auto const glyphs = font.getGlyphs(utf8::asUtf8(textEntry.getText()),
            1.0f);

    std::array<float, 4> const c = premultiply(
            textEntry.getBrush()->getColor()).getArray();
    float const texW = static_cast<float>(text.textureSize[0]);
    float const texH = static_cast<float>(text.textureSize[1]);

    TextQuads quads{
        pmr::vector<float>(memory),
        pmr::vector<uint32_t>(memory)
    };
    quads.vertices.reserve(glyphs.size() * 4 * textVertexFloats);
    quads.indices.reserve(glyphs.size() * 6);

    auto pushVertex = [&](float x, float y, float u, float v)
    {
        quads.vertices.insert(quads.vertices.end(),
                { c[0], c[1], c[2], c[3], x / sf, y / sf, 0.0f, u, v });
    };

    for (auto const& glyph : glyphs)
    {
        Vector2f const pen = (toDevice * glyph.position) * sf;
        float const penX = std::floor(pen[0]);
        float const penY = std::round(pen[1]);
        int const bin = std::min(subpixelBins - 1, static_cast<int>(
                    (pen[0] - penX) * static_cast<float>(subpixelBins)));

        auto slot = text.atlas.get(font, glyph.glyphIndex, pixelSize, bin);
        if (!slot.has_value())
            return std::nullopt;

        AtlasRect const& rect = slot->rect;
        if (rect.width == 0 || rect.height == 0)
            continue;

        DeviceQuad quad;
        quad.x0 = penX + static_cast<float>(slot->bearingX);
        quad.y1 = penY + static_cast<float>(slot->bearingY);
        quad.x1 = quad.x0 + static_cast<float>(rect.width);
        quad.y0 = quad.y1 - static_cast<float>(rect.height);
        quad.u0 = static_cast<float>(rect.x) / texW;
        quad.u1 = static_cast<float>(rect.x + rect.width) / texW;
        quad.v0 = static_cast<float>(rect.y) / texH;
        quad.v1 = static_cast<float>(rect.y + rect.height) / texH;

        std::optional<DeviceQuad> visible = quad;
        if (deviceClip.has_value())
            visible = clipQuad(quad, *deviceClip);

        if (!visible.has_value())
            continue;

        auto const& q = *visible;
        auto const base = static_cast<uint32_t>(
                quads.vertices.size() / textVertexFloats);

        pushVertex(q.x0, q.y0, q.u0, q.v1);
        pushVertex(q.x1, q.y0, q.u1, q.v1);
        pushVertex(q.x1, q.y1, q.u1, q.v0);
        pushVertex(q.x0, q.y1, q.u0, q.v0);

        quads.indices.insert(quads.indices.end(),
                { base, base + 1, base + 2, base, base + 2, base + 3 });
    }

    return quads;
}

Rect getElementRect(Drawing::Element const& e)
{
    ZoneScoped;
    return std::visit(Overloaded{
            [](Drawing::ShapeElement const& shapeElement) -> Rect
            {
                auto bb = shapeElement.shape.getControlBb();
                if (shapeElement.pen)
                    bb = bb.enlarged(shapeElement.pen->getWidth() / 2.0f);

                return bb;
            },
            [](TextEntry const& textEntry) -> Rect
            {
                return textEntry.getControlBb();
            },
            [](Drawing::ClipElement const& clipElement) -> Rect
            {
                assert(std::abs(clipElement.transform.getRotation()) < 0.0001f);

                return clipElement.clipRect
                    .scaled(clipElement.transform.getScale())
                    .translated(clipElement.transform.getTranslation())
                    ;
            },
            [](Drawing::RegionFill const& regionFill) -> Rect
            {
                return regionFill.region.getBoundingBox();
            },
            [](Drawing::BackdropScope const& scope) -> Rect
            {
                return (scope.transform * Obb(scope.controlBb))
                    .getBoundingRect();
            },
            [](Drawing::EffectElement const& effect) -> Rect
            {
                return (effect.transform * Obb(effect.shape.getControlBb()))
                    .getBoundingRect();
            }
            }, e);
}

// Transform is applied after clipping with rect. Rect is not transformed.
// toRoot is the product of transform and every enclosing one: it maps the
// elements' space to the root of the drawing, where text quads are made.
// scopeIndex is the backdrop scope the elements are directly inside.
pmr::vector<Item> generateMeshesForElements(
        pmr::memory_resource* memory,
        Painter const& painter,
        TextContext& text,
        EffectContext& effects,
        size_t scopeIndex,
        Transform const& toRoot,
        Transform const& transform,
        pmr::vector<Drawing::Element> const& elements,
        Vector2f pointSize,
        Rect const& rect,
        bool clip
        )
{
    ZoneScoped;
    if (elements.empty())
        return pmr::vector<Item>(memory);

    ase::Vector2f newPointSize = pointSize / transform.getScale();

    pmr::vector<Item> items(memory);
    items.reserve(elements.size());

    for (auto const& element : elements)
    {
        Rect elementRect = getElementRect(element);
        if (!elementRect.overlaps(rect))
            continue;

        pmr::vector<Item> elementItems = std::visit(Overloaded{
                [&](Drawing::ShapeElement const& shapeElement)
                -> pmr::vector<Item>
                {
                    return generateMeshesForShapeElement(
                            memory, shapeElement, newPointSize, rect, clip);
                },
                [&](TextEntry const& textEntry) -> pmr::vector<Item>
                {
                    auto quads = generateTextQuads(memory, text, toRoot,
                            textEntry,
                            clip ? std::optional<Rect>(rect) : std::nullopt);

                    if (quads.has_value())
                    {
                        pmr::vector<Item> result(memory);
                        if (!quads->indices.empty())
                            result.push_back(std::move(*quads));

                        return result;
                    }

                    auto shape = Shape(textEntry.getFont().textToPath(
                                memory,
                                utf8::asUtf8(textEntry.getText()),
                                1.0f,
                                ase::Vector2f(0.0f, 0.0f)
                                ));

                    auto element = Drawing::ShapeElement {
                        textEntry.getTransform() * std::move(shape),
                        textEntry.getBrush(),
                        textEntry.getPen()
                        };

                    return generateMeshesForShapeElement(memory, element,
                            newPointSize, rect, clip);
                },
                [&](Drawing::ClipElement const& clipElement)
                    -> pmr::vector<Item>
                {
                    auto clipTransformInverse =
                        clipElement.transform.inverse();

                    assert(std::abs(clipTransformInverse.getRotation())
                            < 0.0001f);

                    auto rectInClipElementSpace = rect
                        .scaled(clipTransformInverse.getScale())
                        .translated(clipTransformInverse.getTranslation())
                        ;

                    return generateMeshesForElements(
                            memory,
                            painter,
                            text,
                            effects,
                            scopeIndex,
                            toRoot * clipElement.transform,
                            clipElement.transform,
                            clipElement.subDrawing->elements,
                            newPointSize,
                            clipElement.clipRect.intersected(
                                rectInClipElementSpace),
                            true
                            );
                },
                [&](Drawing::RegionFill const& regionFill) -> pmr::vector<Item>
                {
                    pmr::vector<Item> result(memory);

                    result.push_back(generateMeshForRegion(
                            memory,
                            regionFill.region,
                            regionFill.brush,
                            rect,
                            clip));

                    return result;
                },
                [&](Drawing::BackdropScope const& scope) -> pmr::vector<Item>
                {
                    assert(!clip || isUnrotated(scope.transform));

                    auto rectInScopeSpace = (scope.transform.inverse()
                            * Obb(rect)).getBoundingRect();

                    size_t const index = effects.scopes.size();
                    effects.scopes.push_back(Scope{});

                    pmr::vector<Item> result(memory);
                    result.push_back(ScopeEntry{ index });

                    auto inner = generateMeshesForElements(
                            memory,
                            painter,
                            text,
                            effects,
                            index,
                            toRoot * scope.transform,
                            scope.transform,
                            scope.subDrawing->elements,
                            newPointSize,
                            rectInScopeSpace,
                            clip
                            );

                    for (auto&& item : inner)
                        result.push_back(std::move(item));

                    return result;
                },
                [&](Drawing::EffectElement const& effect) -> pmr::vector<Item>
                {
                    assert(effect.effect);
                    pmr::vector<Item> result(memory);

                    Rect const bounds = clip
                        ? elementRect.intersected(rect)
                        : elementRect;

                    PixelRect const deviceBounds = toDevicePixels(
                            (toRoot * Obb(bounds)).getBoundingRect(),
                            effects.scalingFactor)
                        .clamped(effects.screenSize);

                    if (deviceBounds.isEmpty())
                        return result;

                    std::vector<Vector2f> triangles = triangulateToDevice(
                            memory, effect.shape, toRoot * effect.transform,
                            newPointSize / effect.transform.getScale(),
                            effects.scalingFactor);

                    if (triangles.empty())
                        return result;

                    EffectRegions regions = deriveRegions(*effect.effect,
                            deviceBounds, effects.screenSize);

                    Scope& owner = effects.scopes[scopeIndex];
                    owner.used = true;
                    owner.backdropRegion = owner.backdropRegion.unionWith(
                            regions.backdropRegion);

                    result.push_back(EffectDraw{
                            effect.effect.get(),
                            scopeIndex,
                            std::move(regions),
                            std::move(triangles)
                            });

                    return result;
                }
                }, element);

        for (auto&& item : elementItems)
        {
            if (auto* mesh = std::get_if<SoftMesh>(&item))
                items.push_back(transform * std::move(*mesh));
            else
                items.push_back(std::move(item));
        }
    }

    return items;
}

void renderElements(ase::CommandBuffer& commandBuffer,
        ase::RenderContext& context, ase::Framebuffer& framebuffer,
        Painter const& painter, pmr::vector<Element>&& elements)
{
    ZoneScoped;

    // Opaque elements go first in any order, as the depth test settles
    // them; blended elements stay in paint order, batched only while the
    // pipeline and textures stay the same.
    std::stable_sort(elements.begin(), elements.end(),
            [](Element const& a, Element const& b)
            {
                return !a.pipeline.isBlendEnabled()
                    && b.pipeline.isBlendEnabled();
            });

    std::vector<float> resultVertices;
    std::vector<uint32_t> resultIndices;

    Element const* previous = nullptr;

    size_t vertexCount = 0u;
    size_t indexCount = 0u;
    for (auto const& element : elements)
    {
        vertexCount += element.vertices.size();
        indexCount += element.indices.size();
    }

    resultVertices.reserve(vertexCount);
    resultIndices.reserve(indexCount);

    auto pushElements = [&](Element const& element)
    {
        float const z = resultVertices[vertexZ];

        auto vb = context.makeVertexBuffer();
        auto ib = context.makeIndexBuffer();

        commandBuffer.pushUpload(
                vb,
                ase::Buffer(std::move(resultVertices)),
                ase::Usage::StreamDraw
                );

        commandBuffer.pushUpload(
                ib,
                ase::Buffer(std::move(resultIndices)),
                ase::Usage::StreamDraw
                );

        commandBuffer.push(framebuffer, element.pipeline,
                painter.getUniformSet(), std::move(vb), std::move(ib),
                element.textures, z);

        resultVertices.clear();
        resultIndices.clear();
    };

    for (auto const& element : elements)
    {
        bool const batchChanged = previous
                && (previous->pipeline != element.pipeline
                        || previous->textures != element.textures);

        if (!resultVertices.empty()
                && !resultIndices.empty()
                && batchChanged)
        {
            pushElements(*previous);
        }

        auto indexOffset = static_cast<unsigned int>(
                resultVertices.size() / element.floatsPerVertex);

        resultVertices.insert(resultVertices.end(),
                element.vertices.begin(), element.vertices.end());

        for (auto index : element.indices)
            resultIndices.push_back(indexOffset + index);

        previous = &element;
    }

    if (previous && !resultVertices.empty() && !resultIndices.empty())
        pushElements(*previous);
}

// z descends in paint order over all of the frame's total meshes, of which
// first precede items, so the depth test keeps the order across segments.
pmr::vector<Element> generateElements(pmr::memory_resource* memory,
        Painter const& painter, TextContext const& text,
        pmr::vector<Item>&& items, size_t first, size_t total)
{
    ZoneScoped;
    float step = 0.5f / (float)(total + 1u);

    pmr::vector<Element> elements(memory);
    elements.reserve(items.size());

    uint32_t i = 1;
    for (auto&& item : items)
    {
        float const z = 1.0f - (float)(first + i) * step;

        elements.push_back(std::visit(Overloaded{
                [&](SoftMesh const& mesh) -> Element
                {
                    return Element{
                        painter.getPipeline(mesh.getBrush()),
                        {},
                        vertexFloats,
                        generateVertices(memory, mesh, z),
                        mesh.getIndices()
                        };
                },
                [&](TextQuads& quads) -> Element
                {
                    for (size_t j = vertexZ; j < quads.vertices.size();
                            j += textVertexFloats)
                    {
                        quads.vertices[j] = z;
                    }

                    return Element{
                        painter.getTextPipeline(),
                        { text.texture },
                        textVertexFloats,
                        std::move(quads.vertices),
                        std::move(quads.indices)
                        };
                },
                [&](EffectDraw const&) -> Element
                {
                    throw std::logic_error("an effect is not an element");
                },
                [&](ScopeEntry const&) -> Element
                {
                    throw std::logic_error("a scope entry is not an element");
                }
                }, item));
        ++i;
    }

    return elements;
}

// The meshes gathered so far must become draws before each snapshot and
// each effect: the emitter's barriers order only what is already pushed.
void drawItems(pmr::memory_resource* memory, ase::CommandBuffer& commandBuffer,
        ase::RenderContext& context, ase::Framebuffer& framebuffer,
        Painter const& painter, TextContext const& text,
        std::vector<Scope> const& scopes, ScratchPlan const& plan,
        EffectEmitter* emitter, pmr::vector<Item>&& items)
{
    ZoneScoped;

    size_t total = 0;
    for (auto const& item : items)
    {
        if (isMesh(item))
            ++total;
    }

    pmr::vector<Item> segment(memory);
    size_t first = 0;
    size_t gathered = 0;

    auto flushSegment = [&]()
    {
        if (segment.empty())
            return;

        pmr::vector<Element> elements = generateElements(memory, painter,
                text, std::move(segment), first, total);
        segment.clear();
        first = gathered;

        renderElements(commandBuffer, context, framebuffer, painter,
                std::move(elements));
    };

    size_t effectIndex = 0;
    for (auto&& item : items)
    {
        if (isMesh(item))
        {
            segment.push_back(std::move(item));
            ++gathered;
        }
        else if (auto const* entry = std::get_if<ScopeEntry>(&item))
        {
            Scope const& scope = scopes[entry->scope];
            if (!scope.used)
                continue;

            assert(emitter);
            flushSegment();
            emitter->emitSnapshot(plan.snapshots[scope.snapshot].slot,
                    scope.backdropRegion);
        }
        else if (auto const* draw = std::get_if<EffectDraw>(&item))
        {
            assert(emitter);
            flushSegment();
            emitter->emitEffect(*draw->effect, draw->regions,
                    plan.effects[effectIndex++], draw->triangles);
        }
    }

    flushSegment();
}

} // anonymous namespace

void render(pmr::memory_resource* memory,
        ase::CommandBuffer& commandBuffer, ase::RenderContext& context,
        TargetImage& target,
        float scalingFactor,
        avg::Painter& painter,
        avg::Drawing const& drawing)
{
    ZoneScoped;
    assert(drawing.getPendingShadows().empty());

    ase::Vector2i const size = target.getSize();

    GlyphAtlas& atlas = painter.getGlyphAtlas();
    atlas.beginFrame();

    TextContext text{
        atlas,
        scalingFactor,
        atlas.getTexture(),
        atlas.getTextureSize()
    };

    auto const pixelSize = ase::Vector2f{1.0f, 1.0f};
    float const resPerPixel = std::max(2.0f, 4.0f / scalingFactor);
    auto const pointSize = pixelSize / resPerPixel;
    avg::Vector2f sizef((float)size[0], (float)size[1]);

    Rect rect(Vector2f(0.0f, 0.0f), sizef / scalingFactor);

    // The root is the implicit scope every effect outside one reads.
    EffectContext effects{ scalingFactor, size, {} };
    effects.scopes.push_back(Scope{});

    pmr::vector<Item> items(memory);
    items.push_back(ScopeEntry{ 0 });

    pmr::vector<Item> content = generateMeshesForElements(memory, painter,
            text, effects, 0, Transform(), Transform(), drawing.getElements(),
            pointSize, rect, false);

    for (auto&& item : content)
        items.push_back(std::move(item));

    // The glyph uploads precede this frame's draws in the one buffer, so a
    // later render() into the same buffer cannot overwrite texels these
    // draws still sample.
    atlas.endFrame(commandBuffer);

    ScratchTimeline timeline;
    for (auto const& item : items)
    {
        if (auto const* entry = std::get_if<ScopeEntry>(&item))
        {
            Scope& scope = effects.scopes[entry->scope];
            if (scope.used)
                scope.snapshot = timeline.addSnapshot();
        }
        else if (auto const* draw = std::get_if<EffectDraw>(&item))
        {
            timeline.addEffect(*draw->effect,
                    effects.scopes[draw->scope].snapshot);
        }
    }

    if (timeline.getEffectCount() == 0)
    {
        drawItems(memory, commandBuffer, context, target.getFramebuffer(),
                painter, text, effects.scopes, ScratchPlan(), nullptr,
                std::move(items));
        return;
    }

    ScratchPlan const plan = planScratch(timeline);
    EffectEmitter emitter = painter.makeEffectEmitter(commandBuffer, target);

    drawItems(memory, commandBuffer, context, target.getFramebuffer(),
            painter, text, effects.scopes, plan, &emitter, std::move(items));
}

} // namespace
