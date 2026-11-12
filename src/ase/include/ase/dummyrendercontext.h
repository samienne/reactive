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

        /**
         * @brief Every language, since nothing is compiled.
         */
        std::vector<ShaderLanguage> const& getShaderLanguages()
            const override;

        std::shared_ptr<ShaderImpl> makeShaderImpl(
                ShaderStage stage,
                ShaderLanguage language,
                Buffer const& source) override;

        std::shared_ptr<ProgramImpl> makeProgramImpl(
                Shader const& vertexShader,
                Shader const& fragmentShader,
                ShaderInterface iface) override;

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
                VertexSpec spec,
                PipelineDesc desc) override;

        std::shared_ptr<UniformSetImpl> makeUniformSetImpl(
                ShaderInterface iface) override;

        /**
         * @brief 256, the strictest alignment a GL implementation may
         * require.
         */
        size_t getUniformBufferOffsetAlignment() const override;

        /**
         * @brief The identity, since nothing is drawn.
         */
        Matrix4f getClipSpaceAdjustment() const override;

    private:
        // Upward-strong ref to the platform this context was made by; keeps the
        // platform alive for as long as any window holds this context.
        std::shared_ptr<PlatformImpl> platform_;

        // One persistent main queue, shared across every window frame rather
        // than recreated per submit.
        std::shared_ptr<RenderQueueImpl> mainQueue_;
    };
} // namespace ase
