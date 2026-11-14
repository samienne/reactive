#include "rendercontext.h"

#include "renderqueue.h"
#include "renderbufferimpl.h"
#include "renderbuffer.h"
#include "rendercontextimpl.h"
#include "uniformset.h"
#include "buffer.h"
#include "shader.h"
#include "shaderinterface.h"
#include "program.h"
#include "commandbuffer.h"
#include "window.h"
#include "platform.h"

#include "debug.h"

#include <algorithm>
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

RenderQueue RenderContext::getMainRenderQueue()
{
    return RenderQueue(d()->getMainRenderQueue());
}

std::shared_ptr<RenderContextImpl> RenderContext::getSharedImpl() const
{
    return deferred_;
}

RenderQueue RenderContext::getTransferQueue()
{
    return RenderQueue(d()->getTransferQueue());
}

std::vector<ShaderLanguage> const& RenderContext::getShaderLanguages() const
{
    return d()->getShaderLanguages();
}

Shader RenderContext::makeShader(ShaderStage stage, ShaderLanguage language,
        Buffer const& source)
{
    auto const& languages = d()->getShaderLanguages();
    if (std::find(languages.begin(), languages.end(), language)
            == languages.end())
    {
        throw UnsupportedShaderLanguage(language);
    }

    return Shader(d()->makeShaderImpl(stage, language, source));
}

Program RenderContext::makeProgram(Shader vertexShader, Shader fragmentShader,
        ShaderInterface iface)
{
    if (vertexShader.getStage() != ShaderStage::Vertex)
        throw std::invalid_argument("Vertex shader is of another stage");

    if (fragmentShader.getStage() != ShaderStage::Fragment)
        throw std::invalid_argument("Fragment shader is of another stage");

    return Program(d()->makeProgramImpl(vertexShader, fragmentShader,
                std::move(iface)));
}

VertexBuffer RenderContext::makeVertexBuffer()
{
    return VertexBuffer(d()->makeVertexBufferImpl());
}

IndexBuffer RenderContext::makeIndexBuffer()
{
    return d()->makeIndexBufferImpl();
}

UniformBuffer RenderContext::makeUniformBuffer()
{
    return UniformBuffer(d()->makeUniformBufferImpl());
}

Texture RenderContext::makeTexture(Vector2i size, Format format)
{
    return Texture(d()->makeTextureImpl(size, format));
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

UniformSet RenderContext::makeUniformSet(ShaderInterface iface)
{
    return UniformSet(d()->makeUniformSetImpl(std::move(iface)));
}

size_t RenderContext::getUniformBufferOffsetAlignment() const
{
    return d()->getUniformBufferOffsetAlignment();
}

} // namespace ase
