#include "rendering.h"

#include "traceresource.h"
#include "debug.h"

#include <avg/softmesh.h>
#include <avg/region.h>
#include <avg/jointype.h>
#include <avg/endtype.h>
#include <avg/region.h>

#include <ase/uniformbufferrange.h>
#include <ase/uniformset.h>
#include <ase/usage.h>
#include <ase/buffer.h>
#include <ase/matrix.h>
#include <ase/pipeline.h>
#include <ase/rendercontext.h>
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
#include <cstdint>
#include <optional>

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

// Vertex: rgbaxyzuv
using Vertex = std::array<float, 9>;
using Vertices = pmr::vector<Vertex>;

struct Element
{
    ase::Pipeline pipeline;
    bool opaque;
    std::optional<ase::Texture> texture;
    Vertices vertices;
    pmr::vector<uint32_t> indices;
};

//using Element = std::pair<ase::Pipeline, Vertices>;

Color premultiply(Color c)
{
    return Color(
            c.getAlpha() * c.getRed(),
            c.getAlpha() * c.getGreen(),
            c.getAlpha() * c.getBlue(),
            c.getAlpha()
            );
}

Vertices generateVertices(pmr::memory_resource* memory,
        SoftMesh const& mesh, float z)
{
    ZoneScoped;
    auto const& vertices = mesh.getVertices();
    Transform const& t = mesh.getTransform();
    std::array<float, 4> c = premultiply(mesh.getBrush().getColor())
        .getArray();

    Matrix2f rs = t.getRsMatrix();

    auto const& image = mesh.getBrush().getImage();
    Rect const& imageRect = mesh.getImageRect();
    bool const bottomFirst = image
        && image->getRowOrder() == Image::RowOrder::BottomFirst;
    float const width = std::max(imageRect.getWidth(), 1e-6f);
    float const height = std::max(imageRect.getHeight(), 1e-6f);

    auto toVertex = [&](Vector2f const& v)
    {
        Vector2f p = t.getTranslation() + rs * Vector2f(v[0], v[1]);

        float u = (v[0] - imageRect.getLeft()) / width;
        float up = (v[1] - imageRect.getBottom()) / height;
        float tv = bottomFirst ? up : 1.0f - up;

        return Vertex{{c[0], c[1], c[2], c[3], p[0], p[1], z, u, tv}};
    };

    Vertices result(memory);
    result.reserve(vertices.size());
    for (auto&& v : vertices)
        result.push_back(toVertex(v));

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

    Rect imageRect = brush.getImage() ? region.getBoundingBox() : Rect();

    return SoftMesh(std::move(bufs.first), std::move(bufs.second), brush,
            imageRect);
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

pmr::vector<SoftMesh> generateMeshesForShapeElement(
        pmr::memory_resource* memory, Drawing::ShapeElement const& element,
        ase::Vector2f pointSize, Rect const& r, bool clip)
{
    ZoneScoped;
    auto const& shape = element.shape;
    auto const& brush = element.brush;
    auto const& pen = element.pen;

    pmr::vector<SoftMesh> result(memory);

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
            }
            }, e);
}

// Transform is applied after clipping with rect. Rect is not transformed.
pmr::vector<SoftMesh> generateMeshesForElements(
        pmr::memory_resource* memory,
        Painter const& painter,
        Transform const& transform,
        pmr::vector<Drawing::Element> const& elements,
        Vector2f pointSize,
        Rect const& rect,
        bool clip
        )
{
    ZoneScoped;
    if (elements.empty())
        return pmr::vector<SoftMesh>(memory);

    ase::Vector2f newPointSize = pointSize / transform.getScale();

    pmr::vector<SoftMesh> meshes(memory);
    meshes.reserve(elements.size());

    for (auto const& element : elements)
    {
        Rect elementRect = getElementRect(element);
        if (!elementRect.overlaps(rect))
            continue;

        pmr::vector<SoftMesh> elementMeshes = std::visit(Overloaded{
                [&](Drawing::ShapeElement const& shapeElement)
                -> pmr::vector<SoftMesh>
                {
                    return generateMeshesForShapeElement(
                            memory, shapeElement, newPointSize, rect, clip);
                },
                [&](TextEntry const& textEntry) -> pmr::vector<SoftMesh>
                {
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
                [&](Drawing::ClipElement const& clipElement) -> pmr::vector<SoftMesh>
                {
                    auto clipTransformInverse = clipElement.transform.inverse();

                    assert(std::abs(clipTransformInverse.getRotation()) < 0.0001f);

                    auto rectInClipElementSpace = rect
                        .scaled(clipTransformInverse.getScale())
                        .translated(clipTransformInverse.getTranslation())
                        ;

                    return generateMeshesForElements(
                            memory,
                            painter,
                            clipElement.transform,
                            clipElement.subDrawing->elements,
                            newPointSize,
                            clipElement.clipRect.intersected(rectInClipElementSpace),
                            true
                            );
                },
                [&](Drawing::RegionFill const& regionFill) -> pmr::vector<SoftMesh>
                {
                    pmr::vector<SoftMesh> result(memory);

                    result.push_back(generateMeshForRegion(
                            memory,
                            regionFill.region,
                            regionFill.brush,
                            rect,
                            clip));

                    return result;
                }
                }, element);

        for (auto&& mesh : elementMeshes)
            meshes.push_back(transform * std::move(mesh));
    }

    return meshes;
}

void renderElements(ase::CommandBuffer& commandBuffer,
        ase::RenderContext& context, ase::Framebuffer& framebuffer,
        Painter const& painter, pmr::vector<Element>&& elements)
{
    ZoneScoped;
    // Blended draws do not write depth, so any opaque draw after them would
    // cover them; draw the opaque ones first and the rest in paint order.
    auto compare = [](Element const& a, Element const& b)
    {
        return a.opaque && !b.opaque;
    };

    std::stable_sort(elements.begin(), elements.end(), compare);

    std::vector<Vertex> resultVertices;
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

    auto pushElements = [&](Element const& batch)
    {
        float const z = resultVertices[0][6];

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

        std::vector<ase::Texture> textures;
        if (batch.texture)
            textures.push_back(*batch.texture);

        commandBuffer.push(framebuffer, batch.pipeline,
                painter.getUniformSet(), std::move(vb), std::move(ib),
                std::move(textures), z);

        resultVertices.clear();
        resultIndices.clear();
    };

    for (auto const& element : elements)
    {
        bool const batchChanged = previous
            && (previous->pipeline != element.pipeline
                    || previous->texture != element.texture);

        if (batchChanged && !resultVertices.empty() && !resultIndices.empty())
            pushElements(*previous);

        if (batchChanged || !previous)
            previous = &element;

        auto indexOffset = static_cast<unsigned int>(resultVertices.size());

        for (auto const& v : element.vertices)
            resultVertices.push_back(v);

        for (auto index : element.indices)
            resultIndices.push_back(indexOffset + index);
    }

    if (previous && !resultVertices.empty() && !resultIndices.empty())
        pushElements(*previous);
}

pmr::vector<Element> generateElements(pmr::memory_resource* memory,
        Painter const& painter, pmr::vector<SoftMesh> const& meshes)
{
    ZoneScoped;
    float step = 0.5f / (float)(meshes.size() + 1u);

    pmr::vector<Element> elements(memory);
    elements.reserve(meshes.size());

    uint32_t i = 1;
    for (auto const& mesh : meshes)
    {
        auto const& image = mesh.getBrush().getImage();

        elements.push_back(Element{
                    painter.getPipeline(mesh.getBrush()),
                    painter.isOpaque(mesh.getBrush()),
                    image ? std::make_optional(image->getTexture())
                        : std::nullopt,
                    generateVertices(memory, mesh, 1.0f - (float)i * step),
                    mesh.getIndices()
                    });
        ++i;
    }

    return elements;
}

} // anonymous namespace

void render(pmr::memory_resource* memory,
        ase::CommandBuffer& commandBuffer, ase::RenderContext& context,
        ase::Framebuffer& framebuffer, ase::Vector2i size,
        float scalingFactor,
        avg::Painter const& painter,
        avg::Drawing const& drawing)
{
    ZoneScoped;

    auto const pixelSize = ase::Vector2f{1.0f, 1.0f};
    float const resPerPixel = std::max(2.0f, 4.0f / scalingFactor);
    auto const pointSize = pixelSize / resPerPixel;
    avg::Vector2f sizef((float)size[0], (float)size[1]);

    Rect rect(Vector2f(0.0f, 0.0f), sizef);

    pmr::vector<SoftMesh> meshes = generateMeshesForElements(memory, painter,
            Transform(), drawing.getElements(), pointSize, rect, false);

    pmr::vector<Element> elements = generateElements(memory, painter, meshes);

    renderElements(commandBuffer, context, framebuffer, painter,
            std::move(elements));
}

} // namespace

