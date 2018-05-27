#pragma once

#include "vector.h"
#include "usage.h"
#include "format.h"

#include <btl/visibility.h>

#include <memory>

namespace ase
{
    class ProgramImpl;
    class VertexShaderImpl;
    class FragmentShaderImpl;
    class VertexBufferImpl;
    class IndexBufferImpl;
    class TextureImpl;
    class RenderContextImpl;
    class RenderTargetImpl;
    class RenderTargetObjectImpl;
    class RenderContext;
    class Buffer;

    /**
     * @brief Abstract base class for all platforms
     */
    class BTL_VISIBLE Platform
    {
    public:
        virtual ~Platform() = default;
        virtual RenderContext& getDefaultContext() = 0;
        virtual RenderContext const& getDefaultContext() const = 0;

    private:
        friend class Program;
        friend class VertexShader;
        friend class FragmentShader;
        friend class IndexBuffer;
        friend class VertexBuffer;
        friend class Texture;
        friend class RenderContext;
        friend class RenderTarget;
        friend class RenderTargetObject;

        virtual std::shared_ptr<ProgramImpl> makeProgramImpl(
                RenderContext& context, VertexShaderImpl const& vertexShader,
                FragmentShaderImpl const& fragmentShader) = 0;

        virtual std::shared_ptr<VertexShaderImpl> makeVertexShaderImpl(
                RenderContext& context, std::string const& source) = 0;

        virtual std::shared_ptr<FragmentShaderImpl> makeFragmentShaderImpl(
                RenderContext& context, std::string const& source) = 0;

        virtual std::shared_ptr<VertexBufferImpl> makeVertexBufferImpl(
                RenderContext& context, Buffer const& buffer, Usage usage) = 0;

        virtual std::shared_ptr<IndexBufferImpl> makeIndexBufferImpl(
                RenderContext& context, Buffer const& buffer, Usage usage) = 0;

        virtual std::shared_ptr<TextureImpl> makeTextureImpl(
                RenderContext& context, Vector2i const& size, Format format,
                Buffer const& buffer) = 0;

        virtual std::shared_ptr<RenderContextImpl> makeRenderContextImpl() = 0;

        virtual std::shared_ptr<RenderTargetObjectImpl>
            makeRenderTargetObjectImpl(RenderContext& context) = 0;
    };
}

