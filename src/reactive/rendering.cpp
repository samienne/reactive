#include "rendering.h"

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
#include <btl/option.h>

#include <pmr/vector.h>
#include <pmr/unsynchronized_pool_resource.h>
#include <pmr/monotonic_buffer_resource.h>
#include <pmr/new_delete_resource.h>

namespace reactive
{

namespace
{

// Vertex: rgbaxyz
using Vertex = std::array<float, 7>;
using Vertices = pmr::vector<Vertex>;
using Element = std::pair<ase::Pipeline, Vertices>;

avg::Color premultiply(avg::Color c)
{
    return avg::Color(
            c.getAlpha() * c.getRed(),
            c.getAlpha() * c.getGreen(),
            c.getAlpha() * c.getBlue(),
            c.getAlpha()
            );
}

Vertices generateVertices(pmr::memory_resource* memory,
        avg::SoftMesh const& mesh, float z)
{
    auto const& vertices = mesh.getVertices();
    avg::Transform const& t = mesh.getTransform();
    std::array<float, 4> c = premultiply(mesh.getBrush().getColor())
        .getArray();

    avg::Matrix2f rs = t.getRsMatrix();

    auto toVertex = [z, &t, &c, &rs](std::array<float, 2> const& v)
    {
        avg::Vector2f p = t.getTranslation() + rs * avg::Vector2f(v[0], v[1]);

        return Vertex{{c[0], c[1], c[2], c[3], p[0], p[1], z}};
    };

    Vertices result(memory);
    result.reserve(vertices.size());
    for (auto&& v : vertices)
        result.push_back(toVertex(v));

    return result;
}

avg::SoftMesh generateMesh(pmr::memory_resource* memory,
        avg::Region const& region, avg::Brush const& brush,
        avg::Rect const& r, bool clip)
{
    auto bufs = std::make_pair(
            pmr::vector<ase::Vector2f>(memory),
            pmr::vector<uint16_t>(memory)
            );

    if (clip)
        bufs = region.getClipped(r).triangulate(memory);
    else
        bufs = region.triangulate(memory);

    avg::Color color = premultiply(brush.getColor());
    auto toVertex = [](ase::Vector2f v)
    {
        auto vertex = std::array<float, 2>( { { v[0], v[1]} } );

        return vertex;
    };

    pmr::vector<std::array<float, 2>> vertices(memory);
    vertices.reserve(bufs.first.size());
    for (auto&& v : bufs.first)
        vertices.push_back(toVertex(v));

    return avg::SoftMesh(std::move(vertices), brush);
}

avg::SoftMesh generateMesh(pmr::memory_resource* memory,
        avg::Path const& path, avg::Brush const& brush,
        ase::Vector2f pixelSize, float resPerPixel,
        avg::Rect const& r, bool clip)
{
    pmr::monotonic_buffer_resource mono(memory);

    avg::Region region = path.fillRegion(&mono, avg::FILL_EVENODD,
            pixelSize, resPerPixel);

    return generateMesh(memory, region, brush, r, clip);
}

avg::SoftMesh generateMesh(pmr::memory_resource* memory, avg::Path const& path,
        avg::Pen const& pen, ase::Vector2f pixelSize, int resPerPixel,
        avg::Rect const& r, bool clip)
{
    pmr::monotonic_buffer_resource mono(memory);

    avg::Region region = path.offsetRegion(&mono, pen.getJoinType(),
            pen.getEndType(), pen.getWidth(), pixelSize, resPerPixel);

    return generateMesh(memory, region, pen.getBrush(), r, clip);
}

pmr::vector<avg::SoftMesh> generateMeshes(pmr::memory_resource* memory,
        avg::Shape const& shape, ase::Vector2f pixelSize, float resPerPixel,
        avg::Rect const& r, bool clip)
{
    auto const& path = shape.getPath();
    auto const& brush = shape.getBrush();
    auto const& pen = shape.getPen();

    if (path.isEmpty())
        return pmr::vector<avg::SoftMesh>(memory);

    pmr::vector<avg::SoftMesh> result(memory);

    if (brush.valid())
    {
        bool needClip = !path.getControlBb().isFullyContainedIn(r);
        result.push_back(generateMesh(memory, path, *brush, pixelSize,
                    resPerPixel, r, clip && needClip));
    }

    if (pen.valid())
    {
        avg::Rect penRect = path.getControlBb().enlarged(pen->getWidth());
        bool needClip = !penRect.isFullyContainedIn(r);

        result.push_back(generateMesh(memory, path, *pen, pixelSize,
                    resPerPixel, r, clip && needClip));
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
    {
        auto const& clip = e.get<avg::Drawing::ClipElement>();

        assert(std::abs(clip.transform.getRotation()) < 0.0001f);

        return clip.clipRect
            .scaled(clip.transform.getScale())
            .translated(clip.transform.getTranslation())
            ;
    }
    else
        assert(false);

    return avg::Rect();
}

// Transform is applied after clipping with rect. Rect is not transformed.
pmr::vector<avg::SoftMesh> generateMeshes(
        pmr::memory_resource* memory,
        avg::Painter const& painter,
        avg::Transform const& transform,
        pmr::vector<avg::Drawing::Element> const& elements,
        avg::Vector2f pixelSize,
        float resPerPixel,
        avg::Rect const& rect,
        bool clip
        )
{
    if (elements.empty())
        return pmr::vector<avg::SoftMesh>(memory);

    ase::Vector2f newPixelSize = pixelSize / transform.getScale();

    pmr::vector<avg::SoftMesh> meshes(memory);
    meshes.reserve(elements.size());

    for (auto const& element : elements)
    {
        avg::Rect elementRect = getElementRect(element);
        if (!elementRect.overlaps(rect))
            continue;

        pmr::vector<avg::SoftMesh> elementMeshes(memory);

        if (element.is<avg::Shape>())
        {
            auto const& shape = element.get<avg::Shape>();
            elementMeshes = generateMeshes(memory, shape, newPixelSize,
                    resPerPixel, rect, clip);
        }
        else if (element.is<avg::TextEntry>())
        {
            auto const& text = element.get<avg::TextEntry>();
            auto shape = avg::Shape(memory)
                .setPath(text.getFont().textToPath(
                            memory,
                            utf8::asUtf8(text.getText()),
                            1.0f,
                            ase::Vector2f(0.0f, 0.0f)
                            ))
                .setBrush(text.getBrush())
                .setPen(text.getPen());

            elementMeshes = generateMeshes(
                    memory,
                    text.getTransform() * shape,
                    newPixelSize, resPerPixel, rect, clip
                    );
        }
        else if (element.is<avg::Drawing::ClipElement>())
        {
            auto const& clipElement = element.get<avg::Drawing::ClipElement>();

            auto clipTransformInverse = clipElement.transform.inverse();

            assert(std::abs(clipTransformInverse.getRotation()) < 0.0001f);

            auto rectInClipElementSpace = rect
                .scaled(clipTransformInverse.getScale())
                .translated(clipTransformInverse.getTranslation())
                ;

            elementMeshes = generateMeshes(
                    memory,
                    painter,
                    clipElement.transform,
                    clipElement.subDrawing->elements,
                    newPixelSize,
                    resPerPixel,
                    clipElement.clipRect.intersected(rectInClipElementSpace),
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

void renderElements(ase::CommandBuffer& commandBuffer,
        ase::RenderContext& context, ase::Framebuffer& framebuffer,
        avg::Painter const& painter, pmr::vector<Element>&& elements)
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
            float const z = resultVertices[0][2];

            auto vb = context.makeVertexBuffer(ase::Buffer(
                        std::move(resultVertices)), ase::Usage::StreamDraw);

            commandBuffer.push(framebuffer, pipeline, painter.getUniformSet(),
                    std::move(vb), btl::none, {}, z);

            resultVertices.clear();
        }

        if (!previousPipeline.valid())
            previousPipeline = btl::just(pipeline);
    }
}

pmr::vector<Element> generateElements(pmr::memory_resource* memory,
        avg::Painter const& painter, pmr::vector<avg::SoftMesh> const& meshes)
{
    float step = 0.5f / (float)(meshes.size() + 1u);

    pmr::vector<Element> elements(memory);
    elements.reserve(meshes.size());

    uint32_t i = 1;
    for (auto const& mesh : meshes)
    {
        elements.push_back(std::make_pair(painter.getPipeline(mesh.getBrush()),
                    generateVertices(memory,
                        mesh, 1.0f - (float)i * step)));
        ++i;
    }

    return elements;
}

} // anonymous namespace

class TraceResource : public pmr::memory_resource
{
public:
    TraceResource(pmr::memory_resource* upstream) :
        upstream_(upstream)
    {
    }

    ~TraceResource()
    {
        assert(allocations_.empty());
    }

    std::size_t getAllocationCount() const
    {
        return allocationCount_;
    }

    std::size_t getTotalAllocation() const
    {
        return totalAllocation_;
    }

    std::size_t getCurrentAllocation() const
    {
        return currentAllocation_;
    }

    std::size_t getMaxAllocation() const
    {
        return maxAllocation_;
    }

    std::size_t getConsecutiveAllocDealloc() const
    {
        return consecutiveAllocDealloc_;
    }

    std::size_t getConsecutiveAllocDeallocBytes() const
    {
        return consecutiveAllocDeallocBytes_;
    }

private:
    void* do_allocate(std::size_t size, std::size_t alignment) override
    {
        totalAllocation_ += size;
        allocationCount_ += 1;
        currentAllocation_ += size;

        maxAllocation_ = std::max(currentAllocation_, maxAllocation_);

        void* p = upstream_->allocate(size, alignment);
        allocations_.push_back(Allocation{ p, size, alignment });

        previous_ = p;

        return p;
    }

    void do_deallocate(void* p, std::size_t size, std::size_t alignment) override
    {
        if (p == previous_)
        {
            consecutiveAllocDealloc_ += 1;
            consecutiveAllocDeallocBytes_ += size;
        }

        currentAllocation_ -= size;

        bool found = false;
        for (auto i = allocations_.begin(); i != allocations_.end(); ++i)
        {
            Allocation& a = *i;
            if (a.p == p)
            {
                if (a.size != size)
                {
                    std::cout << "size mismatch: " << a.size << " != " << size
                        << std::endl;
                }

                if (a.alignment != alignment)
                {
                    std::cout << "alignment mismatch: "
                        << a.alignment << " != " << alignment << std::endl;
                }

                assert(a.size == size && a.alignment == alignment);
                found = true;
                allocations_.erase(i);
                break;
            }
        }

        //assert(found);
        if (!found)
        {
            std::cout << "didn't find allocation " << p << std::endl;
        }

        return upstream_->deallocate(p, size, alignment);
    }

    bool do_is_equal(memory_resource const& rhs) const override
    {
        return this == &rhs;
    }

    memory_resource* upstream_;

    struct Allocation
    {
        void* p;
        std::size_t size;
        std::size_t alignment;
    };

    std::vector<Allocation> allocations_;
    std::size_t allocationCount_ = 0;
    std::size_t totalAllocation_ = 0;
    std::size_t currentAllocation_ = 0;
    std::size_t maxAllocation_ = 0;
    std::size_t consecutiveAllocDealloc_ = 0;
    std::size_t consecutiveAllocDeallocBytes_ = 0;
    void* previous_ = nullptr;
};

void render(pmr::memory_resource* memory,
        ase::CommandBuffer& commandBuffer, ase::RenderContext& context,
        ase::Framebuffer& framebuffer, ase::Vector2i size,
        float scalingFactor,
        avg::Painter const& painter,
        avg::Drawing const& drawing)
{
    auto const pixelSize = ase::Vector2f{1.0f, 1.0f};
    float const resPerPixel = 4.0f / scalingFactor;
    avg::Vector2f sizef((float)size[0], (float)size[1]);

    avg::Rect rect(avg::Vector2f(0.0f, 0.0f), sizef);

    auto meshes = generateMeshes(memory, painter, avg::Transform(),
            drawing.getElements(), pixelSize, resPerPixel, rect, false);

    auto elements = generateElements(memory, painter, meshes);

    renderElements(commandBuffer, context, framebuffer, painter,
            std::move(elements));
}

} // namespace

