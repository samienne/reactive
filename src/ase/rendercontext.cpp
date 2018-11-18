#include "rendercontext.h"

#include "rendercontextimpl.h"
#include "rendertargetobject.h"
#include "buffer.h"
#include "fragmentshader.h"
#include "vertexshader.h"
#include "commandbuffer.h"
#include "window.h"
#include "platform.h"

#include "debug.h"

#include <stdexcept>

namespace ase
{

RenderContext::RenderContext(Platform& platform,
        std::shared_ptr<RenderContextImpl> impl) :
    deferred_(impl),
    platform_(platform)
{
}

RenderContext::RenderContext(Platform& platform) :
    deferred_(platform.makeRenderContextImpl()),
    platform_(platform)
{
}

RenderContext::~RenderContext()
{
}

void RenderContext::flush()
{
    if (d())
        d()->flush();
}

void RenderContext::finish()
{
    if (d())
        d()->finish();
}

void RenderContext::present(Window& window)
{
    if (d())
        d()->present(window);
}

void RenderContext::submit(CommandBuffer&& commands)
{
    if (d())
        d()->submit(std::move(commands));
}

VertexShader RenderContext::makeVertexShader(std::string const& source)
{
    return platform_.makeVertexShaderImpl(*this, source);
}

FragmentShader RenderContext::makeFragmentShader(std::string const& source)
{
    return platform_.makeFragmentShaderImpl(*this, source);
}

Program RenderContext::makeProgram(VertexShader vertexShader,
        FragmentShader fragmentShader)
{
    return platform_.makeProgramImpl(*this, std::move(vertexShader),
            std::move(fragmentShader));
}

VertexBuffer RenderContext::makeVertexBuffer(Buffer buffer, Usage usage)
{
    return platform_.makeVertexBufferImpl(*this, std::move(buffer), usage);
}

IndexBuffer RenderContext::makeIndexBuffer(Buffer buffer, Usage usage)
{
    return platform_.makeIndexBufferImpl(*this, std::move(buffer), usage);
}

Texture RenderContext::makeTexture(Vector2i size, Format format, Buffer buffer)
{
    return Texture(platform_.makeTextureImpl(*this, size, format,
                std::move(buffer)));
}

RenderTargetObject RenderContext::makeRenderTargetObject()
{
    return RenderTargetObject(platform_.makeRenderTargetObjectImpl(*this));
}

Pipeline RenderContext::makePipeline(Program program, VertexSpec vertexSpec)
{
    return Pipeline(platform_.makePipeline(*this, std::move(program),
                std::move(vertexSpec)));
}

Pipeline RenderContext::makePipelineWithBlend(Program program,
        VertexSpec vertexSpec, BlendMode srcFactor, BlendMode dstFactor)
{
    return Pipeline(platform_.makePipelineWithBlend(*this,
                std::move(program), std::move(vertexSpec), srcFactor,
                dstFactor));
}

Platform& RenderContext::getPlatform() const
{
    return platform_;
}

} // namespace ase

