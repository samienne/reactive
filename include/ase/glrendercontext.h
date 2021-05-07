#pragma once

#include "glframebuffer.h"
#include "globjectmanager.h"
#include "glrenderstate.h"

#include "rendercontextimpl.h"
#include "vector.h"

#include "asevisibility.h"

#include <btl/option.h>

namespace ase
{
    class VertexSpec;
    class GlPlatform;
    class GlDispatchedContext;

    class ASE_EXPORT GlRenderContext : public RenderContextImpl
    {
    public:
        GlRenderContext(
                GlPlatform& platform,
                std::shared_ptr<GlDispatchedContext> fgContext,
                std::shared_ptr<GlDispatchedContext> bgContext
                );

        ~GlRenderContext() override;

        virtual void present(Dispatched dispatched, Window& window) = 0;

        // From RenderContextImpl
        void submit(CommandBuffer&& commands) override;
        void flush() override;
        void finish() override;

        GlFramebuffer const& getDefaultFramebuffer() const;

    protected:
        friend class GlProgram;
        friend class GlTexture;
        friend class GlBuffer;
        friend class GlShader;
        friend class GlPlatform;
        friend class GlPipeline;
        friend class GlFramebuffer;
        friend class GlObjectManager;
        friend class GlUniformBuffer;
        friend class GlRenderbuffer;

        void dispatch(std::function<void(GlFunctions const&)>&& func);
        void dispatchBg(std::function<void(GlFunctions const&)>&& func);
        void wait() const;
        void waitBg() const;

        GlFramebuffer& getSharedFramebuffer(Dispatched);
        void setViewport(Dispatched, Vector2i size);
        void clear(Dispatched, GLbitfield mask);

        // From RenderContextImpl
        std::shared_ptr<ProgramImpl> makeProgramImpl(
                VertexShader const& vertexShader,
                FragmentShader const& fragmentShader) override;

        std::shared_ptr<VertexShaderImpl> makeVertexShaderImpl(
                std::string const& source) override;

        std::shared_ptr<FragmentShaderImpl> makeFragmentShaderImpl(
                std::string const& source) override;

        std::shared_ptr<VertexBufferImpl> makeVertexBufferImpl(
                Buffer const& buffer,
                Usage usage) override;

        std::shared_ptr<IndexBufferImpl> makeIndexBufferImpl(
                Buffer const& buffer,
                Usage usage) override;

        std::shared_ptr<UniformBufferImpl> makeUniformBufferImpl(
                Buffer buffer, Usage usage) override;

        std::shared_ptr<TextureImpl> makeTextureImpl(
                Vector2i const& size,
                Format format,
                Buffer const& buffer) override;

        std::shared_ptr<RenderbufferImpl> makeRenderbufferImpl(
                Vector2i const& size,
                Format format) override;

        std::shared_ptr<FramebufferImpl>
            makeFramebufferImpl() override;

        std::shared_ptr<PipelineImpl> makePipeline(
                Program program,
                VertexSpec spec) override;

        std::shared_ptr<PipelineImpl> makePipelineWithBlend(
                Program program,
                VertexSpec spec,
                BlendMode srcFactor,
                BlendMode dstFactor) override;

        std::shared_ptr<UniformSetImpl> makeUniformSetImpl() override;

    protected:
        GlDispatchedContext const& getFgContext() const;
        GlDispatchedContext const& getBgContext() const;

    private:
        GlPlatform& platform_;
        std::shared_ptr<GlDispatchedContext> fgContext_;
        std::shared_ptr<GlDispatchedContext> bgContext_;
        GlObjectManager objectManager_;
        GlRenderState renderState_;
        GlFramebuffer defaultFramebuffer_;
        GlFramebuffer sharedFramebuffer_;
        Vector2i viewportSize_;
    };
}

