#pragma once

#include "blendmode.h"
#include "usage.h"
#include "vector.h"
#include "format.h"

#include <memory>

namespace ase
{
    class GlRenderContext;
    class GlProgram;
    class GlVertexShader;
    class GlFragmentShader;
    class GlVertexBuffer;
    class GlIndexBuffer;
    class GlUniformBuffer;
    class GlTexture;
    class GlFramebuffer;
    class GlPipeline;
    class GlUniformSet;
    class GlRenderbuffer;

    class VertexShader;
    class FragmentShader;
    class VertexBuffer;
    class Buffer;
    class Program;
    class VertexSpec;

    class GlObjectManager
    {
    public:
        GlObjectManager(GlRenderContext& context);

        std::shared_ptr<GlProgram> makeProgram(
                VertexShader const& vertexShader,
                FragmentShader const& fragmentShader);

        std::shared_ptr<GlVertexShader> makeVertexShader(
                std::string const& source);

        std::shared_ptr<GlFragmentShader> makeFragmentShader(
                std::string const& source);

        std::shared_ptr<GlVertexBuffer> makeVertexBuffer();

        std::shared_ptr<GlIndexBuffer> makeIndexBuffer();

        std::shared_ptr<GlUniformBuffer> makeUniformBuffer();

        std::shared_ptr<GlTexture> makeTexture(
                Vector2i const& size,
                Format format);

        std::shared_ptr<GlRenderbuffer> makeRenderbuffer(
                Vector2i const& size, Format format);

        std::shared_ptr<GlFramebuffer> makeFramebuffer();

        std::shared_ptr<GlPipeline> makePipeline(
                Program program,
                VertexSpec spec);

        std::shared_ptr<GlPipeline> makePipelineWithBlend(
                Program program,
                VertexSpec spec,
                BlendMode srcFactor,
                BlendMode dstFactor);

        std::shared_ptr<GlUniformSet> makeUniformSet();

    private:
        GlRenderContext& context_;
    };
} // namespace ase
