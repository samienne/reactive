#include "rendercontext.h"

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

VertexBuffer RenderContext::makeVertexBuffer(Buffer buffer, Usage usage)
{
    return VertexBuffer(d()->makeVertexBufferImpl(std::move(buffer), usage));
}

IndexBuffer RenderContext::makeIndexBuffer(Buffer buffer, Usage usage)
{
    return d()->makeIndexBufferImpl(std::move(buffer), usage);
}

UniformBuffer RenderContext::makeUniformBuffer(Buffer buffer, Usage usage)
{
    return UniformBuffer(d()->makeUniformBufferImpl(std::move(buffer), usage));
}

Texture RenderContext::makeTexture(Vector2i size, Format format, Buffer buffer)
{
    return Texture(d()->makeTextureImpl(size, format,
                std::move(buffer)));
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

