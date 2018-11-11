#pragma once

#include "rendercontext.h"
#include "platform.h"

#include <btl/visibility.h>

#include <memory>

namespace ase
{
    class GlRenderContext;

    /**
     * @brief Abstract base class for all OpenGl platforms
     */
    class BTL_VISIBLE GlPlatform : public Platform
    {
    public:
        GlPlatform();
        virtual ~GlPlatform();

    private:
        friend class GlShader;
        friend class GlBuffer;

        // From Platform
        std::shared_ptr<ProgramImpl> makeProgramImpl(
                RenderContext& context, VertexShader const& vertexShader,
                FragmentShader const& fragmentShader) override;

        std::shared_ptr<VertexShaderImpl> makeVertexShaderImpl(
                RenderContext& context, std::string const& source) override;

        std::shared_ptr<FragmentShaderImpl> makeFragmentShaderImpl(
                RenderContext& context, std::string const& source) override;

        std::shared_ptr<VertexBufferImpl> makeVertexBufferImpl(
                RenderContext& context, Buffer const& buffer,
                Usage usage) override;

        std::shared_ptr<IndexBufferImpl> makeIndexBufferImpl(
                RenderContext& context, Buffer const& buffer,
                Usage usage) override;

        std::shared_ptr<TextureImpl> makeTextureImpl(
                RenderContext& context, Vector2i const& size,
                Format format, Buffer const& buffer) override;

        std::shared_ptr<RenderTargetObjectImpl>
            makeRenderTargetObjectImpl(RenderContext& context) override;

        std::shared_ptr<PipelineImpl> makePipeline(
                RenderContext& context,
                Program program,
                VertexSpec spec) override;

        std::shared_ptr<PipelineImpl> makePipelineWithBlend(
                RenderContext& context,
                Program program,
                VertexSpec spec,
                BlendMode srcFactor,
                BlendMode dstFactor) override;
    };
}

