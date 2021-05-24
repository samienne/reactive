#include "rendercontext.h"

#include "renderbufferimpl.h"
#include "renderbuffer.h"
#include "rendercontextimpl.h"
#include "uniformset.h"
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

RenderContext::RenderContext(std::shared_ptr<RenderContextImpl> impl) :
    deferred_(std::move(impl))
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

void RenderContext::submit(CommandBuffer&& commands)
{
    if (d())
        d()->submit(std::move(commands));
}

VertexShader RenderContext::makeVertexShader(std::string const& source)
{
    return d()->makeVertexShaderImpl(source);
}

FragmentShader RenderContext::makeFragmentShader(std::string const& source)
{
    return d()->makeFragmentShaderImpl(source);
}

Program RenderContext::makeProgram(VertexShader vertexShader,
        FragmentShader fragmentShader)
{
    return d()->makeProgramImpl(std::move(vertexShader),
            std::move(fragmentShader));
}

VertexBuffer RenderContext::makeVertexBuffer()
{
    return VertexBuffer(d()->makeVertexBufferImpl());
}

IndexBuffer RenderContext::makeIndexBuffer(Buffer buffer, Usage usage)
{
    return d()->makeIndexBufferImpl(std::move(buffer), usage);
}

UniformBuffer RenderContext::makeUniformBuffer()
{
    return UniformBuffer(d()->makeUniformBufferImpl());
}

Texture RenderContext::makeTexture(Vector2i size, Format format, Buffer buffer)
{
    return Texture(d()->makeTextureImpl(size, format,
                std::move(buffer)));
}

Renderbuffer RenderContext::makeRenderbuffer(Vector2i size, Format format)
{
    return Renderbuffer(d()->makeRenderbufferImpl(size, format));
}

Framebuffer RenderContext::makeFramebuffer()
{
    return Framebuffer(d()->makeFramebufferImpl());
}

Pipeline RenderContext::makePipeline(Program program, VertexSpec vertexSpec)
{
    return Pipeline(d()->makePipeline(std::move(program),
                std::move(vertexSpec)));
}

Pipeline RenderContext::makePipelineWithBlend(Program program,
        VertexSpec vertexSpec, BlendMode srcFactor, BlendMode dstFactor)
{
    return Pipeline(d()->makePipelineWithBlend(
                std::move(program), std::move(vertexSpec), srcFactor,
                dstFactor));
}

UniformSet RenderContext::makeUniformSet()
{
    return UniformSet(d()->makeUniformSetImpl());
}

} // namespace ase

