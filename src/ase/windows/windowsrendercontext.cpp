#include "windowsrendercontext.h"

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
    return nullptr;
}

std::shared_ptr<VertexShaderImpl> WindowsRenderContext::makeVertexShaderImpl(
        std::string const& /*source*/)
{
    return nullptr;
}

std::shared_ptr<FragmentShaderImpl> WindowsRenderContext::makeFragmentShaderImpl(
        std::string const& /*source*/)
{
    return nullptr;
}

std::shared_ptr<VertexBufferImpl> WindowsRenderContext::makeVertexBufferImpl(
        Buffer const& /*buffer*/, Usage /*usage*/)
{
    return nullptr;
}

std::shared_ptr<IndexBufferImpl> WindowsRenderContext::makeIndexBufferImpl(
        Buffer const& /*buffer*/, Usage /*usage*/)
{
    return nullptr;
}

std::shared_ptr<UniformBufferImpl> WindowsRenderContext::makeUniformBufferImpl(
        Buffer /*buffer*/, Usage /*usage*/)
{
    return nullptr;
}

std::shared_ptr<TextureImpl> WindowsRenderContext::makeTextureImpl(
        Vector2i const& /*size*/, Format /*format*/,
        Buffer const& /*buffer*/)
{
    return nullptr;
}

std::shared_ptr<FramebufferImpl> WindowsRenderContext::makeFramebufferImpl()
{
    return nullptr;
}

std::shared_ptr<PipelineImpl> WindowsRenderContext::makePipeline(
        Program /*program*/,
        VertexSpec /*spec*/)
{
    return nullptr;
}

std::shared_ptr<PipelineImpl> WindowsRenderContext::makePipelineWithBlend(
        Program /*program*/,
        VertexSpec /*spec*/,
        BlendMode /*srcFactor*/,
        BlendMode /*dstFactor*/)
{
    return nullptr;
}

std::shared_ptr<UniformSetImpl> WindowsRenderContext::makeUniformSetImpl()
{
    return nullptr;
}

} // namespace ase


