#pragma once

#include "blendmode.h"
#include "format.h"
#include "shaderlanguage.h"
#include "shaderstage.h"
#include "usage.h"
#include "vector.h"

#include "asevisibility.h"

#include <vector>
#include <memory>

namespace ase
{
    class Platform;
    class Window;
    class CommandBuffer;

    class ProgramImpl;
    class ShaderImpl;
    class VertexBufferImpl;
    class IndexBufferImpl;
    class UniformBufferImpl;
    class TextureImpl;
    class FramebufferImpl;
    class UniformSetImpl;
    class Buffer;
    class PipelineImpl;
    class Program;
    class Shader;
    class ShaderInterface;
    class VertexSpec;
    class UniformSet;
    class RenderbufferImpl;
    class RenderQueueImpl;

    class ASE_EXPORT RenderContextImpl
    {
    public:
        virtual ~RenderContextImpl() = default;

        virtual std::shared_ptr<RenderQueueImpl> getMainRenderQueue() = 0;
        virtual std::shared_ptr<RenderQueueImpl> getTransferQueue() = 0;

        virtual std::vector<ShaderLanguage> const& getShaderLanguages()
            const = 0;

        virtual std::shared_ptr<ShaderImpl> makeShaderImpl(
                ShaderStage stage,
                ShaderLanguage language,
                Buffer const& source) = 0;

        virtual std::shared_ptr<ProgramImpl> makeProgramImpl(
                Shader const& vertexShader,
                Shader const& fragmentShader,
                ShaderInterface iface) = 0;

        virtual std::shared_ptr<VertexBufferImpl> makeVertexBufferImpl() = 0;

        virtual std::shared_ptr<IndexBufferImpl> makeIndexBufferImpl() = 0;

        virtual std::shared_ptr<UniformBufferImpl> makeUniformBufferImpl() = 0;

        virtual std::shared_ptr<TextureImpl> makeTextureImpl(
                Vector2i const& size, Format format) = 0;

        virtual std::shared_ptr<RenderbufferImpl> makeRenderbufferImpl(
                Vector2i const& size, Format format) = 0;

        virtual std::shared_ptr<FramebufferImpl> makeFramebufferImpl() = 0;

        virtual std::shared_ptr<PipelineImpl> makePipeline(
                Program program,
                VertexSpec spec) = 0;

        virtual std::shared_ptr<PipelineImpl> makePipelineWithBlend(
                Program program,
                VertexSpec spec,
                BlendMode srcFactor,
                BlendMode dstFactor) = 0;

        virtual std::shared_ptr<UniformSetImpl> makeUniformSetImpl() = 0;
    };
}
