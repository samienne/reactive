#include "wglrendercontext.h"

#include "buffer.h"
#include "program.h"
#include "vertexspec.h"

namespace ase
{

void WglRenderContext::submit(CommandBuffer&& /*commands*/)
{
}

void WglRenderContext::flush()
{
}

void WglRenderContext::finish()
{
}

void WglRenderContext::present(Window& /*window*/)
{
}

std::shared_ptr<ProgramImpl> WglRenderContext::makeProgramImpl(
        VertexShader const& /*vertexShader*/,
        FragmentShader const& /*fragmentShader*/)
{
    return nullptr;
}

std::shared_ptr<VertexShaderImpl> WglRenderContext::makeVertexShaderImpl(
        std::string const& /*source*/)
{
    return nullptr;
}

std::shared_ptr<FragmentShaderImpl> WglRenderContext::makeFragmentShaderImpl(
        std::string const& /*source*/)
{
    return nullptr;
}

std::shared_ptr<VertexBufferImpl> WglRenderContext::makeVertexBufferImpl(
        Buffer const& /*buffer*/, Usage /*usage*/)
{
    return nullptr;
}

std::shared_ptr<IndexBufferImpl> WglRenderContext::makeIndexBufferImpl(
        Buffer const& /*buffer*/, Usage /*usage*/)
{
    return nullptr;
}

std::shared_ptr<UniformBufferImpl> WglRenderContext::makeUniformBufferImpl(
        Buffer /*buffer*/, Usage /*usage*/)
{
    return nullptr;
}

std::shared_ptr<TextureImpl> WglRenderContext::makeTextureImpl(
        Vector2i const& /*size*/, Format /*format*/,
        Buffer const& /*buffer*/)
{
    return nullptr;
}

std::shared_ptr<FramebufferImpl> WglRenderContext::makeFramebufferImpl()
{
    return nullptr;
}

std::shared_ptr<PipelineImpl> WglRenderContext::makePipeline(
        Program /*program*/,
        VertexSpec /*spec*/)
{
    return nullptr;
}

std::shared_ptr<PipelineImpl> WglRenderContext::makePipelineWithBlend(
        Program /*program*/,
        VertexSpec /*spec*/,
        BlendMode /*srcFactor*/,
        BlendMode /*dstFactor*/)
{
    return nullptr;
}

std::shared_ptr<UniformSetImpl> WglRenderContext::makeUniformSetImpl()
{
    return nullptr;
}

} // namespace ase

