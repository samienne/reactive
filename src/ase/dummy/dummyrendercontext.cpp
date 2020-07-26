#include "dummyrendercontext.h"

#include "buffer.h"
#include "program.h"
#include "vertexspec.h"

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

void DummyRenderContext::present(Window& /*window*/)
{
}

std::shared_ptr<ProgramImpl> DummyRenderContext::makeProgramImpl(
        VertexShader const& /*vertexShader*/,
        FragmentShader const& /*fragmentShader*/)
{
    return nullptr;
}

std::shared_ptr<VertexShaderImpl> DummyRenderContext::makeVertexShaderImpl(
        std::string const& /*source*/)
{
    return nullptr;
}

std::shared_ptr<FragmentShaderImpl> DummyRenderContext::makeFragmentShaderImpl(
        std::string const& /*source*/)
{
    return nullptr;
}

std::shared_ptr<VertexBufferImpl> DummyRenderContext::makeVertexBufferImpl(
        Buffer const& /*buffer*/, Usage /*usage*/)
{
    return nullptr;
}

std::shared_ptr<IndexBufferImpl> DummyRenderContext::makeIndexBufferImpl(
        Buffer const& /*buffer*/, Usage /*usage*/)
{
    return nullptr;
}

std::shared_ptr<UniformBufferImpl> DummyRenderContext::makeUniformBufferImpl(
        Buffer /*buffer*/, Usage /*usage*/)
{
    return nullptr;
}

std::shared_ptr<TextureImpl> DummyRenderContext::makeTextureImpl(
        Vector2i const& /*size*/, Format /*format*/,
        Buffer const& /*buffer*/)
{
    return nullptr;
}

std::shared_ptr<FramebufferImpl> DummyRenderContext::makeFramebufferImpl()
{
    return nullptr;
}

std::shared_ptr<PipelineImpl> DummyRenderContext::makePipeline(
        Program /*program*/,
        VertexSpec /*spec*/)
{
    return nullptr;
}

std::shared_ptr<PipelineImpl> DummyRenderContext::makePipelineWithBlend(
        Program /*program*/,
        VertexSpec /*spec*/,
        BlendMode /*srcFactor*/,
        BlendMode /*dstFactor*/)
{
    return nullptr;
}

std::shared_ptr<UniformSetImpl> DummyRenderContext::makeUniformSetImpl()
{
    return nullptr;
}

} // namespace ase

