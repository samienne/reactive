#include "rendering.h"

#include "debug.h"

#include <avg/softmesh.h>
#include <avg/region.h>
#include <avg/jointype.h>
#include <avg/endtype.h>
#include <avg/region.h>

#include <ase/buffer.h>
#include <ase/matrix.h>
#include <ase/pipeline.h>
#include <ase/rendercontext.h>
#include <ase/nameduniformbuffer.h>
#include <ase/async.h>

#include <btl/fnv1a.h>
#include <btl/uhash.h>
#include <btl/hash.h>
#include <btl/option.h>

namespace reactive
{

namespace
{

// Vertex: rgbaxyz
using Vertex = std::array<float, 7>;
using Vertices = std::vector<Vertex>;
using Element = std::pair<ase::Pipeline, Vertices>;

Vertices generateVertices(avg::SoftMesh const& mesh, float z)
{
    auto const& vertices = mesh.getVertices();
    avg::Transform const& t = mesh.getTransform();
    std::array<float, 4> const& c = mesh.getBrush().getColor().getArray();

    avg::Matrix2f rs = t.getRsMatrix();

    auto toVertex = [z, &t, &c, &rs](std::array<float, 2> const& v)
    {
        avg::Vector2f p = t.getTranslation() + rs * avg::Vector2f(v[0], v[1]);

        return Vertex{{c[0], c[1], c[2], c[3], p[0], p[1], z}};
    };

    Vertices result;
    result.reserve(vertices.size());
    for (auto&& v : vertices)
        result.push_back(toVertex(v));

    return result;
}

avg::SoftMesh generateMesh(avg::Region const& region, avg::Brush const& brush,
        avg::Rect const& r, bool clip)
{
    std::pair<std::vector<ase::Vector2f>, std::vector<uint16_t> > bufs;
    if (clip)
        bufs = region.getClipped(r).triangulate();
    else
        bufs = region.triangulate();

    avg::Color const& color = brush.getColor();
    auto toVertex = [&color](ase::Vector2f v)
    {
        auto vertex = std::array<float, 2>( { { v[0], v[1]} } );

        return vertex;
    };

    std::vector<std::array<float, 2>> vertices;
    vertices.reserve(bufs.first.size());
    for (auto&& v : bufs.first)
        vertices.push_back(toVertex(v));

    return avg::SoftMesh(std::move(vertices), brush);
}

avg::SoftMesh generateMesh(avg::Path const& path,
        avg::Brush const& brush, ase::Vector2f pixelSize, int resPerPixel,
        avg::Rect const& r, bool clip)
{
    avg::Region region = path.fillRegion(avg::FILL_EVENODD,
            pixelSize, resPerPixel);

    return generateMesh(region, brush, r, clip);
}

avg::SoftMesh generateMesh(avg::Path const& path, avg::Pen const& pen,
        ase::Vector2f pixelSize, int resPerPixel,
        avg::Rect const& r, bool clip)
{
    avg::Region region = path.offsetRegion(avg::JOIN_ROUND, avg::END_OPENROUND,
            pen.getWidth(), pixelSize, resPerPixel);

    return generateMesh(region, pen.getBrush(), r, clip);
}

std::vector<avg::SoftMesh> generateMeshes(avg::Shape const& shape,
        ase::Vector2f pixelSize, int resPerPixel,
        avg::Rect const& r, bool clip)
{
    auto const& path = shape.getPath();
    auto const& brush = shape.getBrush();
    auto const& pen = shape.getPen();

    if (path.isEmpty())
        return {};

    std::vector<avg::SoftMesh> result;

    if (brush.valid())
    {
        bool needClip = !path.getControlBb().isFullyContainedIn(r);
        result.push_back(generateMesh(path, *brush, pixelSize, resPerPixel,
                    r, clip && needClip));
    }

    if (pen.valid())
    {
        avg::Rect penRect = path.getControlBb().enlarged(pen->getWidth());
        bool needClip = !penRect.isFullyContainedIn(r);

        result.push_back(generateMesh(path, *pen, pixelSize, resPerPixel,
                    r, clip && needClip));
    }

    return result;
}

avg::Rect getElementRect(avg::Drawing::Element const& e)
{
    if (e.is<avg::Shape>())
        return e.get<avg::Shape>().getControlBb();
    else if(e.is<avg::TextEntry>())
        return e.get<avg::TextEntry>().getControlBb();
    else if(e.is<avg::Drawing::ClipElement>())
        return e.get<avg::Drawing::ClipElement>().clipRect;
    else
        assert(false);
}

std::vector<avg::SoftMesh> generateMeshes(avg::Painter const& painter,
        avg::Transform const& transform,
        std::vector<avg::Drawing::Element> const& elements,
        avg::Vector2f pixelSize, int resPerPixel,
        avg::Rect const& rect, bool clip)
{
    if (elements.empty())
        return {};

    std::vector<avg::SoftMesh> meshes;
    meshes.reserve(elements.size());

    for (auto const& element : elements)
    {
        if (!getElementRect(element).overlaps(rect))
            continue;

        std::vector<avg::SoftMesh> elementMeshes;

        if (element.is<avg::Shape>())
        {
            auto const& shape = element.get<avg::Shape>();
            elementMeshes = generateMeshes(shape, pixelSize,
                    resPerPixel, rect, clip);
        }
        else if (element.is<avg::TextEntry>())
        {
            auto const& text = element.get<avg::TextEntry>();
            auto shape = avg::Shape()
                .setPath(text.getFont().textToPath(utf8::asUtf8(text.getText()),
                            1.0f, ase::Vector2f(0.0f, 0.0f)))
                .setBrush(text.getBrush())
                .setPen(text.getPen());

            elementMeshes = generateMeshes(
                    text.getTransform() * shape,
                    pixelSize, resPerPixel, rect, clip
                    );
        }
        else if (element.is<avg::Drawing::ClipElement>())
        {
            auto const& clipElement = element.get<avg::Drawing::ClipElement>();

            elementMeshes = generateMeshes(
                    painter,
                    clipElement.transform,
                    clipElement.subDrawing->elements,
                    pixelSize / clipElement.transform.getScale(),
                    resPerPixel,
                    clipElement.clipRect,
                    true
                    );
        }
        else
            assert(false); // Unknown element type

        for (auto&& mesh : elementMeshes)
            meshes.push_back(transform * std::move(mesh));
    }

    return meshes;
}

void render(ase::RenderContext& context,
        ase::RenderTarget& target, std::vector<Element>&& elements)
{
    auto compare = [](std::pair<ase::Pipeline, Vertices> const& a,
            std::pair<ase::Pipeline, Vertices> const& b)
    {
        return a.first < b.first;
    };

    std::sort(elements.begin(), elements.end(), compare);

    std::vector<Vertex> resultVertices;
    btl::option<ase::Pipeline> previousPipeline;

    size_t count = 0u;
    for (auto const& element : elements)
        count += element.second.size();
    resultVertices.reserve(count);

    auto&& size = target.getResolution();

    ase::Matrix4f m = ase::Matrix4f::Identity();
    m(0,0) = 2.0f/(float)size[0];
    m(1,1) = 2.0f/(float)size[1];
    m(0,3) = -1.0f;
    m(1,3) = -1.0f;

    ase::NamedUniformBuffer nub;
    nub.uniformMatrix4fv("worldViewProj", 1, m.data());

    ase::IndexBuffer ib;

    for (auto i = elements.begin(); i != elements.end(); ++i)
    {
        auto const& element = *i;
        auto next = i+1;
        auto const& pipeline = element.first;
        auto const& vertices = element.second;

        for (auto const& v : vertices)
            resultVertices.push_back(v);

        bool const outOfElements = next == elements.end();
        bool const pipelineChanged = previousPipeline.valid()
                && *previousPipeline != pipeline;

        if (!resultVertices.empty() && (outOfElements || pipelineChanged))
        {
            ase::UniformBuffer ub(pipeline.getProgram(), nub);
            float const z = resultVertices[0][2];
            ase::VertexBuffer vb(context, ase::Buffer(
                        std::move(resultVertices)), ase::Usage::StreamDraw,
                    ase::Async());
            ase::RenderCommand command(pipeline, ub, vb, ib,
                    {ase::Texture()}, z);
            target.push(std::move(command));
            resultVertices.clear();
        }

        if (!previousPipeline.valid())
            previousPipeline = btl::just(pipeline);
    }
}

std::vector<Element> generateElements(avg::Painter const& painter,
        std::vector<avg::SoftMesh> const& meshes)
{
    float step = 0.5f / (float)(meshes.size() + 1u);

    std::vector<Element> elements;
    elements.reserve(meshes.size());

    uint32_t i = 1;
    for (auto const& mesh : meshes)
    {
        elements.push_back(std::make_pair(painter.getPipeline(mesh.getBrush()),
                    generateVertices(mesh, 1.0f - (float)i * step)));
        ++i;
    }

    return elements;
}

} // anonymous namespace

void render(ase::RenderContext& context, ase::RenderTarget& target,
        avg::Painter const& painter, avg::Drawing const& drawing)
{
    auto const pixelSize = ase::Vector2f{1.0f, 1.0f};
    int const resPerPixel = 4;

    avg::Rect rect(
            avg::Vector2f(0.0f, 0.0f),
            avg::Vector2f(target.getResolution()[0], target.getResolution()[1])
            );

    auto meshes = generateMeshes(painter, avg::Transform(),
            drawing.getElements(), pixelSize, resPerPixel, rect, false);

    auto elements = generateElements(painter, meshes);

    render(context, target, std::move(elements));
}

avg::Path makeRect(float width, float height)
{
    float w = width / 2.0f;
    float h = height / 2.0f;

    return avg::Path(avg::PathSpec()
            .start(ase::Vector2f(-w, -h))
            .lineTo(ase::Vector2f(w, -h))
            .lineTo(ase::Vector2f(w, h))
            .lineTo(ase::Vector2f(-w, h))
            .close()
            );
}

avg::Shape makeShape(avg::Path const& path,
        btl::option<avg::Brush> const& brush,
        btl::option<avg::Pen> const& pen)
{
    return avg::Shape()
        .setPath(path)
        .setBrush(brush)
        .setPen(pen);
}

} // namespace

