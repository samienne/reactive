#pragma once

#include "blendmode.h"
#include "pipelinedesc.h"
#include "usage.h"
#include "vector.h"
#include "format.h"
#include "shaderstage.h"

#include <memory>

namespace ase
{
    class GlRenderContext;
    class GlProgram;
    class GlShader;
    class GlVertexBuffer;
    class GlIndexBuffer;
    class GlUniformBuffer;
    class GlTexture;
    class GlFramebuffer;
    class GlPipeline;
    class GlUniformSet;
    class GlRenderbuffer;

    class Shader;
    class ShaderInterface;
    class VertexBuffer;
    class Buffer;
    class Program;
    class VertexSpec;

    class GlObjectManager
    {
    public:
        GlObjectManager(GlRenderContext& context);

        std::shared_ptr<GlShader> makeShader(
                ShaderStage stage,
                Buffer const& source);

        std::shared_ptr<GlProgram> makeProgram(
                Shader const& vertexShader,
                Shader const& fragmentShader,
                ShaderInterface iface);

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
                VertexSpec spec,
                PipelineDesc desc);

        std::shared_ptr<GlUniformSet> makeUniformSet(ShaderInterface iface);

    private:
        GlRenderContext& context_;
    };
} // namespace ase
