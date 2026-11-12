#include "dummyrendercontext.h"

#include "dummyrenderqueue.h"
#include "dummyrenderbuffer.h"
#include "dummyuniformset.h"
#include "dummypipeline.h"
#include "dummyframebuffer.h"
#include "dummytexture.h"
#include "dummyuniformbuffer.h"
#include "dummyindexbuffer.h"
#include "dummyvertexbuffer.h"
#include "dummyshader.h"
#include "dummyprogram.h"

#include "buffer.h"
#include "program.h"
#include "shader.h"
#include "shaderinterface.h"
#include "vertexspec.h"
#include <memory>

namespace ase
{

DummyRenderContext::DummyRenderContext(std::shared_ptr<PlatformImpl> platform) :
    platform_(std::move(platform)),
    mainQueue_(std::make_shared<DummyRenderQueue>())
{
}

std::shared_ptr<RenderQueueImpl> DummyRenderContext::getMainRenderQueue()
{
    return mainQueue_;
}

std::shared_ptr<RenderQueueImpl> DummyRenderContext::getTransferQueue()
{
    return std::make_shared<DummyRenderQueue>();
}

std::vector<ShaderLanguage> const& DummyRenderContext::getShaderLanguages()
    const
{
    static std::vector<ShaderLanguage> const languages = {
        ShaderLanguage::Glsl330,
        ShaderLanguage::GlslEs300,
        ShaderLanguage::Glsl450,
        ShaderLanguage::SpirV,
        ShaderLanguage::Wgsl,
        ShaderLanguage::Msl
    };

    return languages;
}

std::shared_ptr<ShaderImpl> DummyRenderContext::makeShaderImpl(
        ShaderStage stage,
        ShaderLanguage /*language*/,
        Buffer const& /*source*/)
{
    return std::make_shared<DummyShader>(stage);
}

std::shared_ptr<ProgramImpl> DummyRenderContext::makeProgramImpl(
        Shader const& /*vertexShader*/,
        Shader const& /*fragmentShader*/,
        ShaderInterface iface)
{
    return std::make_shared<DummyProgram>(std::move(iface));
}

std::shared_ptr<VertexBufferImpl> DummyRenderContext::makeVertexBufferImpl()
{
    return std::make_shared<DummyVertexBuffer>();
}

std::shared_ptr<IndexBufferImpl> DummyRenderContext::makeIndexBufferImpl()
{
    return std::make_shared<DummyIndexBuffer>();
}

std::shared_ptr<UniformBufferImpl> DummyRenderContext::makeUniformBufferImpl()
{
    return std::make_shared<DummyUniformBuffer>();
}

std::shared_ptr<TextureImpl> DummyRenderContext::makeTextureImpl(
        Vector2i const& size, Format format)
{
    return std::make_shared<DummyTexture>(size, format);
}

std::shared_ptr<RenderbufferImpl> DummyRenderContext::makeRenderbufferImpl(
        Vector2i const& size,
        Format format)
{
    return std::make_shared<DummyRenderBuffer>(format, size);
}

std::shared_ptr<FramebufferImpl> DummyRenderContext::makeFramebufferImpl()
{
    return std::make_shared<DummyFramebuffer>();
}

std::shared_ptr<PipelineImpl> DummyRenderContext::makePipeline(
        Program program,
        VertexSpec spec)
{
    return std::make_shared<DummyPipeline>(std::move(program),
            std::move(spec), false);
}

std::shared_ptr<PipelineImpl> DummyRenderContext::makePipelineWithBlend(
        Program program,
        VertexSpec spec,
        BlendMode /*srcFactor*/,
        BlendMode /*dstFactor*/)
{
    return std::make_shared<DummyPipeline>(std::move(program),
            std::move(spec), true);
}

std::shared_ptr<UniformSetImpl> DummyRenderContext::makeUniformSetImpl(
        ShaderInterface iface)
{
    return std::make_shared<DummyUniformSet>(std::move(iface));
}

size_t DummyRenderContext::getUniformBufferOffsetAlignment() const
{
    return 256;
}

} // namespace ase
