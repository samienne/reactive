#pragma once

#include "usage.h"
#include "format.h"
#include "vector.h"
#include "blendmode.h"

#include "asevisibility.h"

#include <functional>
#include <vector>
#include <memory>

namespace ase
{
    class Window;
    class RenderContextImpl;
    class Platform;
    class CommandBuffer;
    class VertexShader;
    class FragmentShader;
    class Program;
    class VertexBuffer;
    class IndexBuffer;
    class UniformBuffer;
    class Texture;
    class Buffer;
    class Framebuffer;
    class Pipeline;
    class VertexSpec;
    class UniformSet;
    class Renderbuffer;

    /**
     * @brief RenderContext
     *
     * Loading resources on a RenderContext makes them immediately
     * available for that context, although the upload might not have finished.
     * Using a resource with an unfinished upload might cause an implicit
     * wait until the resource is uploaded.
     *
     * Calling flush() will wait until all previous data uploads have finished
     * on that RenderContext.
     *
     * Flushing
     *
     * 1. Submit -- Sends commands to the rendering context.
     * 2. Flush  -- Waits for the submitted commands to be sent to the hw.
     *              context. Also waits for data upload to finish.
     * 3. Finish -- Waits for previously flushed contents to be finished.
     *              Makes all the previous data affected by commands and
     *              uploads available for other RenderContexts on the
     *              same Platform.
     *
     */
    class ASE_EXPORT RenderContext
    {
    public:
        RenderContext(std::shared_ptr<RenderContextImpl> impl);
        RenderContext(RenderContext const&) = delete;
        RenderContext(RenderContext&&) = default;
        ~RenderContext();

        RenderContext& operator=(RenderContext const&) = delete;
        RenderContext& operator=(RenderContext&&) = delete;

        void flush();
        void finish();
        void submit(CommandBuffer&& renderQueue);

        VertexShader makeVertexShader(std::string const& source);
        FragmentShader makeFragmentShader(std::string const& source);
        Program makeProgram(VertexShader vertexShader,
                FragmentShader fragmentShader);
        VertexBuffer makeVertexBuffer();
        IndexBuffer makeIndexBuffer(Buffer buffer, Usage usage);
        UniformBuffer makeUniformBuffer();
        Texture makeTexture(Vector2i size, Format format, Buffer buffer);
        Renderbuffer makeRenderbuffer(Vector2i size, Format format);
        Framebuffer makeFramebuffer();
        Pipeline makePipeline(Program program, VertexSpec vertexSpec);
        Pipeline makePipelineWithBlend(Program program, VertexSpec vertexSpec,
                BlendMode srcFactor, BlendMode dstFactor);

        UniformSet makeUniformSet();

        template <class T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

        template <class T>
        T& getImpl()
        {
            return reinterpret_cast<T&>(*d());
        }

    private:
        std::shared_ptr<RenderContextImpl> deferred_;
        inline RenderContextImpl* d() { return deferred_.get(); }
        inline RenderContextImpl const* d() const { return deferred_.get(); }
    };
}

