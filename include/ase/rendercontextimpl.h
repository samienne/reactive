#pragma once

#include "blendmode.h"
#include "format.h"
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
    class VertexShaderImpl;
    class FragmentShaderImpl;
    class VertexBufferImpl;
    class IndexBufferImpl;
    class UniformBufferImpl;
    class TextureImpl;
    class FramebufferImpl;
    class UniformSetImpl;
    class Buffer;
    class PipelineImpl;
    class Program;
    class FragmentShader;
    class VertexShader;
    class VertexSpec;
    class UniformSet;
    class RenderbufferImpl;

    class ASE_EXPORT RenderContextImpl
    {
    public:
        virtual ~RenderContextImpl() = default;

        virtual void submit(CommandBuffer&& commands) = 0;
        virtual void flush() = 0;
        virtual void finish() = 0;

        virtual std::shared_ptr<ProgramImpl> makeProgramImpl(
                VertexShader const& vertexShader,
                FragmentShader const& fragmentShader) = 0;

        virtual std::shared_ptr<VertexShaderImpl> makeVertexShaderImpl(
                std::string const& source) = 0;

        virtual std::shared_ptr<FragmentShaderImpl> makeFragmentShaderImpl(
                std::string const& source) = 0;

        virtual std::shared_ptr<VertexBufferImpl> makeVertexBufferImpl() = 0;

        virtual std::shared_ptr<IndexBufferImpl> makeIndexBufferImpl(
                Buffer const& buffer, Usage usage) = 0;

        virtual std::shared_ptr<UniformBufferImpl> makeUniformBufferImpl() = 0;

        virtual std::shared_ptr<TextureImpl> makeTextureImpl(
                Vector2i const& size, Format format,
                Buffer const& buffer) = 0;

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

