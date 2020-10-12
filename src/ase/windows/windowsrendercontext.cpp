#include "windowsrendercontext.h"

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

namespace ase
{

void WindowsRenderContext::submit(CommandBuffer&& /*commands*/)
{
}

void WindowsRenderContext::flush()
{
}

void WindowsRenderContext::finish()
{
}

void WindowsRenderContext::present(Window& /*window*/)
{
}

std::shared_ptr<ProgramImpl> WindowsRenderContext::makeProgramImpl(
        VertexShader const& /*vertexShader*/,
        FragmentShader const& /*fragmentShader*/)
{
    return std::make_shared<DummyProgram>();
}

std::shared_ptr<VertexShaderImpl> WindowsRenderContext::makeVertexShaderImpl(
        std::string const& /*source*/)
{
    return std::make_shared<DummyVertexShader>();
}

std::shared_ptr<FragmentShaderImpl> WindowsRenderContext::makeFragmentShaderImpl(
        std::string const& /*source*/)
{
    return std::make_shared<DummyFragmentShader>();
}

std::shared_ptr<VertexBufferImpl> WindowsRenderContext::makeVertexBufferImpl(
        Buffer const& /*buffer*/, Usage /*usage*/)
{
    return std::make_shared<DummyVertexBuffer>();
}

std::shared_ptr<IndexBufferImpl> WindowsRenderContext::makeIndexBufferImpl(
        Buffer const& /*buffer*/, Usage /*usage*/)
{
    return std::make_shared<DummyIndexBuffer>();
}

std::shared_ptr<UniformBufferImpl> WindowsRenderContext::makeUniformBufferImpl(
        Buffer /*buffer*/, Usage /*usage*/)
{
    return std::make_shared<DummyUniformBuffer>();
}

std::shared_ptr<TextureImpl> WindowsRenderContext::makeTextureImpl(
        Vector2i const& /*size*/, Format /*format*/,
        Buffer const& /*buffer*/)
{
    return std::make_shared<DummyTexture>();
}

std::shared_ptr<FramebufferImpl> WindowsRenderContext::makeFramebufferImpl()
{
    return std::make_shared<DummyFramebuffer>();
}

std::shared_ptr<PipelineImpl> WindowsRenderContext::makePipeline(
        Program program,
        VertexSpec /*spec*/)
{
    return std::make_shared<DummyPipeline>(std::move(program));
}

std::shared_ptr<PipelineImpl> WindowsRenderContext::makePipelineWithBlend(
        Program program,
        VertexSpec /*spec*/,
        BlendMode /*srcFactor*/,
        BlendMode /*dstFactor*/)
{
    return std::make_shared<DummyPipeline>(std::move(program));
}

std::shared_ptr<UniformSetImpl> WindowsRenderContext::makeUniformSetImpl()
{
    return std::make_shared<DummyUniformSet>();
}


} // namespace ase


