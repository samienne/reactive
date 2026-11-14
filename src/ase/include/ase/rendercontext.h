#pragma once

#include "usage.h"
#include "format.h"
#include "vector.h"
#include "blendmode.h"
#include "shaderlanguage.h"
#include "shaderstage.h"

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
    class Shader;
    class ShaderInterface;
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
    class RenderQueue;

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

        RenderQueue getMainRenderQueue();
        RenderQueue getTransferQueue();

        /**
         * @brief The shader languages this context compiles, most preferred
         * first.
         */
        std::vector<ShaderLanguage> const& getShaderLanguages() const;

        /**
         * @brief Compiles the source bytes of one stage.
         *
         * Throws UnsupportedShaderLanguage when the language is not in
         * getShaderLanguages(), and std::runtime_error when the source does
         * not compile.
         */
        Shader makeShader(ShaderStage stage, ShaderLanguage language,
                Buffer const& source);

        /**
         * @brief Links a vertex and a fragment shader into a program whose
         * inputs are those the interface declares.
         *
         * Throws std::invalid_argument when a shader is of the wrong stage
         * and std::runtime_error when the program does not link or, where
         * the backend can check, when the compiled program disagrees with
         * the interface.
         */
        Program makeProgram(Shader vertexShader, Shader fragmentShader,
                ShaderInterface iface);

        VertexBuffer makeVertexBuffer();
        IndexBuffer makeIndexBuffer();
        UniformBuffer makeUniformBuffer();
        Texture makeTexture(Vector2i size, Format format);
        Renderbuffer makeRenderbuffer(Vector2i size, Format format);
        Framebuffer makeFramebuffer();
        Pipeline makePipeline(Program program, VertexSpec vertexSpec);
        Pipeline makePipelineWithBlend(Program program, VertexSpec vertexSpec,
                BlendMode srcFactor, BlendMode dstFactor);

        /**
         * @brief A uniform set for the blocks iface declares.
         */
        UniformSet makeUniformSet(ShaderInterface iface);

        /**
         * @brief The alignment, in bytes, a bound uniform buffer range's
         * offset must have on this context.
         */
        size_t getUniformBufferOffsetAlignment() const;

        /** @brief A co-owning handle to this context's implementation.
         *
         * Same-binary only.
         */
        std::shared_ptr<RenderContextImpl> getSharedImpl() const;

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
