#include "dummyrendercontext.h"

#include "dummyrenderbuffer.h"
#include "dummyuniformset.h"
#include "dummypipeline.h"
#include "dummyframebuffer.h"
#include "dummytexture.h"
#include "dummyuniformbuffer.h"
#include "dummyindexbuffer.h"
#include "dummyvertexbuffer.h"
#include "dummyvertexshader.h"
#include "dummyfragmentshader.h"
#include "dummyprogram.h"

#include "buffer.h"
#include "program.h"
#include "vertexspec.h"
#include <memory>

namespace ase
{

void DummyRenderContext::submit(CommandBuffer&& /*commands*/)
{
}

void DummyRenderContext::flush()
{
}

void DummyRenderContext::finish()
{
}

std::shared_ptr<ProgramImpl> DummyRenderContext::makeProgramImpl(
        VertexShader const& /*vertexShader*/,
        FragmentShader const& /*fragmentShader*/)
{
    return std::make_shared<DummyProgram>();
}

std::shared_ptr<VertexShaderImpl> DummyRenderContext::makeVertexShaderImpl(
        std::string const& /*source*/)
{
    return std::make_shared<DummyVertexShader>();
}

std::shared_ptr<FragmentShaderImpl> DummyRenderContext::makeFragmentShaderImpl(
        std::string const& /*source*/)
{
    return std::make_shared<DummyFragmentShader>();
}

std::shared_ptr<VertexBufferImpl> DummyRenderContext::makeVertexBufferImpl(
        Buffer const& /*buffer*/, Usage /*usage*/)
{
    return std::make_shared<DummyVertexBuffer>();
}

std::shared_ptr<IndexBufferImpl> DummyRenderContext::makeIndexBufferImpl(
        Buffer const& /*buffer*/, Usage /*usage*/)
{
    return std::make_shared<DummyIndexBuffer>();
}

std::shared_ptr<UniformBufferImpl> DummyRenderContext::makeUniformBufferImpl(
        Buffer /*buffer*/, Usage /*usage*/)
{
    return std::make_shared<DummyUniformBuffer>();
}

std::shared_ptr<TextureImpl> DummyRenderContext::makeTextureImpl(
        Vector2i const& /*size*/, Format /*format*/,
        Buffer const& /*buffer*/)
{
    return std::make_shared<DummyTexture>();
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
        VertexSpec /*spec*/)
{
    return std::make_shared<DummyPipeline>(std::move(program));
}

std::shared_ptr<PipelineImpl> DummyRenderContext::makePipelineWithBlend(
        Program program,
        VertexSpec /*spec*/,
        BlendMode /*srcFactor*/,
        BlendMode /*dstFactor*/)
{
    return std::make_shared<DummyPipeline>(std::move(program));
}

std::shared_ptr<UniformSetImpl> DummyRenderContext::makeUniformSetImpl()
{
    return std::make_shared<DummyUniformSet>();
}

} // namespace ase

