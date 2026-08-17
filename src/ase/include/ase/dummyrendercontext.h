#pragma once

#include "rendercontextimpl.h"

#include <memory>

namespace ase
{
    class PlatformImpl;

    class ASE_EXPORT DummyRenderContext : public RenderContextImpl
    {
    public:
        /** @brief Bind the context to the platform that made it, co-owning it. */
        explicit DummyRenderContext(std::shared_ptr<PlatformImpl> platform);

        std::shared_ptr<RenderQueueImpl> getMainRenderQueue() override;
        std::shared_ptr<RenderQueueImpl> getTransferQueue() override;

        std::shared_ptr<ProgramImpl> makeProgramImpl(
                VertexShader const& vertexShader,
                FragmentShader const& fragmentShader) override;

        std::shared_ptr<VertexShaderImpl> makeVertexShaderImpl(
                std::string const& source) override;

        std::shared_ptr<FragmentShaderImpl> makeFragmentShaderImpl(
                std::string const& source) override;

        std::shared_ptr<VertexBufferImpl> makeVertexBufferImpl() override;

        std::shared_ptr<IndexBufferImpl> makeIndexBufferImpl() override;

        std::shared_ptr<UniformBufferImpl> makeUniformBufferImpl() override;

        std::shared_ptr<TextureImpl> makeTextureImpl(
                Vector2i const& size, Format format) override;

        std::shared_ptr<RenderbufferImpl> makeRenderbufferImpl(
                Vector2i const& size,
                Format format) override;

        std::shared_ptr<FramebufferImpl> makeFramebufferImpl() override;

        std::shared_ptr<PipelineImpl> makePipeline(
                Program program,
                VertexSpec spec) override;

        std::shared_ptr<PipelineImpl> makePipelineWithBlend(
                Program program,
                VertexSpec spec,
                BlendMode srcFactor,
                BlendMode dstFactor) override;

        std::shared_ptr<UniformSetImpl> makeUniformSetImpl() override;

    private:
        // Upward-strong ref to the platform this context was made by; keeps the
        // platform alive for as long as any window holds this context.
        std::shared_ptr<PlatformImpl> platform_;
    };
} // namespace ase

