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

// Vertex: rgbaxyz
using Vertex = std::array<float, 7>;
using Vertices = pmr::vector<Vertex>;

struct Element
{
    ase::Pipeline pipeline;
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
    auto const& vertices = mesh.getVertices();
    Transform const& t = mesh.getTransform();
    std::array<float, 4> c = premultiply(mesh.getBrush().getColor())
        .getArray();

    Matrix2f rs = t.getRsMatrix();

    auto toVertex = [z, &t, &c, &rs](Vector2f const& v)
    {
        Vector2f p = t.getTranslation() + rs * Vector2f(v[0], v[1]);

        return Vertex{{c[0], c[1], c[2], c[3], p[0], p[1], z}};
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
        Path const& path, Brush const& brush,
        ase::Vector2f pixelSize, float resPerPixel,
        Rect const& r, bool clip)
{
    pmr::monotonic_buffer_resource mono(memory);

    Region region = path.fillRegion(&mono, FILL_EVENODD,
            pixelSize, resPerPixel);

    return generateMeshForRegion(memory, region, brush, r, clip);
}

SoftMesh generateMeshForPen(pmr::memory_resource* memory, Path const& path,
        Pen const& pen, ase::Vector2f pixelSize, float resPerPixel,
        Rect const& r, bool clip)
{
    pmr::monotonic_buffer_resource mono(memory);

    Region region = path.offsetRegion(&mono, pen.getJoinType(),
            pen.getEndType(), pen.getWidth(), pixelSize, resPerPixel);

    return generateMeshForRegion(memory, region, pen.getBrush(), r, clip);
}

pmr::vector<SoftMesh> generateMeshesForShape(pmr::memory_resource* memory,
        Shape const& shape, ase::Vector2f pixelSize, float resPerPixel,
        Rect const& r, bool clip)
{
    auto const& path = shape.getPath();
    auto const& brush = shape.getBrush();
    auto const& pen = shape.getPen();

    if (path.isEmpty())
        return pmr::vector<SoftMesh>(memory);

    pmr::vector<SoftMesh> result(memory);

    if (brush.has_value())
    {
        bool needClip = !path.getControlBb().isFullyContainedIn(r);
        result.push_back(generateMeshForBrush(memory, path, *brush, pixelSize,
                    resPerPixel, r, clip && needClip));
    }

    if (pen.has_value())
    {
        Rect penRect = path.getControlBb().enlarged(pen->getWidth());
        bool needClip = !penRect.isFullyContainedIn(r);

        result.push_back(generateMeshForPen(memory, path, *pen, pixelSize,
                    resPerPixel, r, clip && needClip));
    }

    return result;
}

Rect getElementRect(Drawing::Element const& e)
{
    return std::visit(Overloaded{
            [](Shape const& shape) -> Rect
            {
                return shape.getControlBb();
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
        Vector2f pixelSize,
        float resPerPixel,
        Rect const& rect,
        bool clip
        )
{
    if (elements.empty())
        return pmr::vector<SoftMesh>(memory);

    ase::Vector2f newPixelSize = pixelSize / transform.getScale();

    pmr::vector<SoftMesh> meshes(memory);
    meshes.reserve(elements.size());

    for (auto const& element : elements)
    {
        Rect elementRect = getElementRect(element);
        if (!elementRect.overlaps(rect))
            continue;

        pmr::vector<SoftMesh> elementMeshes = std::visit(Overloaded{
                [&](Shape const& shape) -> pmr::vector<SoftMesh>
                {
                    return generateMeshesForShape(memory, shape, newPixelSize,
                            resPerPixel, rect, clip);
                },
                [&](TextEntry const& textEntry) -> pmr::vector<SoftMesh>
                {
                    auto shape = Shape(memory)
                        .setPath(textEntry.getFont().textToPath(
                                    memory,
                                    utf8::asUtf8(textEntry.getText()),
                                    1.0f,
                                    ase::Vector2f(0.0f, 0.0f)
                                    ))
                        .setBrush(textEntry.getBrush())
                        .setPen(textEntry.getPen());

                    return generateMeshesForShape(
                            memory,
                            textEntry.getTransform() * shape,
                            newPixelSize, resPerPixel, rect, clip
                            );
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
                            newPixelSize,
                            resPerPixel,
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
    auto compare = [](Element const& a, Element const& b)
    {
        return a.pipeline < b.pipeline;
    };

    std::sort(elements.begin(), elements.end(), compare);

    std::vector<Vertex> resultVertices;
    std::vector<uint32_t> resultIndices;

    std::optional<ase::Pipeline> previousPipeline;

    size_t vertexCount = 0u;
    size_t indexCount = 0u;
    for (auto const& element : elements)
    {
        vertexCount += element.vertices.size();
        indexCount += element.indices.size();
    }

    resultVertices.reserve(vertexCount);
    resultIndices.reserve(indexCount);

    for (auto i = elements.begin(); i != elements.end(); ++i)
    {
        auto const& element = *i;
        auto next = i+1;

        uint32_t indexOffset = resultVertices.size();

        for (auto const& v : element.vertices)
            resultVertices.push_back(v);

        for (auto index : element.indices)
            resultIndices.push_back(indexOffset + index);

        bool const outOfElements = next == elements.end();
        bool const pipelineChanged = previousPipeline.has_value()
                && *previousPipeline != element.pipeline;

        if (!resultVertices.empty()
                && !resultIndices.empty()
                && (outOfElements || pipelineChanged))
        {
            float const z = resultVertices[0][2];

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

            commandBuffer.push(framebuffer, element.pipeline, painter.getUniformSet(),
                    std::move(vb), std::move(ib), {}, z);

            resultVertices.clear();
            resultIndices.clear();
        }

        if (!previousPipeline.has_value())
            previousPipeline = element.pipeline;
    }
}

pmr::vector<Element> generateElements(pmr::memory_resource* memory,
        Painter const& painter, pmr::vector<SoftMesh> const& meshes)
{
    float step = 0.5f / (float)(meshes.size() + 1u);

    pmr::vector<Element> elements(memory);
    elements.reserve(meshes.size());

    uint32_t i = 1;
    for (auto const& mesh : meshes)
    {
        elements.push_back(Element{
                    painter.getPipeline(mesh.getBrush()),
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
    auto const pixelSize = ase::Vector2f{1.0f, 1.0f};
    float const resPerPixel = std::max(2.0f, 4.0f / scalingFactor);
    avg::Vector2f sizef((float)size[0], (float)size[1]);

    Rect rect(Vector2f(0.0f, 0.0f), sizef);

    pmr::vector<SoftMesh> meshes = generateMeshesForElements(memory, painter,
            Transform(), drawing.getElements(), pixelSize,
            resPerPixel, rect, false);

    pmr::vector<Element> elements = generateElements(memory, painter, meshes);

    renderElements(commandBuffer, context, framebuffer, painter,
            std::move(elements));
}

} // namespace

