#pragma once

#include "rendercontextimpl.h"

namespace ase
{
    class ASE_EXPORT WindowsRenderContext : public RenderContextImpl
    {
    public:
        void submit(CommandBuffer&& commands) override;
        void flush() override;
        void finish() override;
        void present(Window& window) override;

        std::shared_ptr<ProgramImpl> makeProgramImpl(
                VertexShader const& vertexShader,
                FragmentShader const& fragmentShader) override;

        std::shared_ptr<VertexShaderImpl> makeVertexShaderImpl(
                std::string const& source) override;

        std::shared_ptr<FragmentShaderImpl> makeFragmentShaderImpl(
                std::string const& source) override;

        std::shared_ptr<VertexBufferImpl> makeVertexBufferImpl(
                Buffer const& buffer, Usage usage) override;

        std::shared_ptr<IndexBufferImpl> makeIndexBufferImpl(
                Buffer const& buffer, Usage usage) override;

        std::shared_ptr<UniformBufferImpl> makeUniformBufferImpl(
                Buffer buffer, Usage usage) override;

        std::shared_ptr<TextureImpl> makeTextureImpl(
                Vector2i const& size, Format format,
                Buffer const& buffer) override;

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
    };
} // namespace ase

