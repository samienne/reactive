#include "wglrendercontext.h"

#include "wglwindow.h"
#include "wgldispatchedcontext.h"
#include "wglplatform.h"

#include "window.h"

#undef min

/*
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
*/

/*
#include "window.h"
#include "buffer.h"
#include "program.h"
#include "vertexspec.h"
*/

#include <iostream>

namespace ase
{

WglRenderContext::WglRenderContext(WglPlatform& platform) :
    GlRenderContext(platform,
            std::make_shared<WglDispatchedContext>(platform),
            std::make_shared<WglDispatchedContext>(platform)
            ),
    platform_(platform)
{
}

#if 0
void WglRenderContext::submit(CommandBuffer&& /*commands*/)
{
}

void WglRenderContext::flush()
{
}

void WglRenderContext::finish()
{
}
#endif

void WglRenderContext::present(Window& window)
{
    window.getImpl<WglWindow>().present();
}

#if 0
std::shared_ptr<ProgramImpl> WglRenderContext::makeProgramImpl(
        VertexShader const& /*vertexShader*/,
        FragmentShader const& /*fragmentShader*/)
{
    return std::make_shared<DummyProgram>();
}

std::shared_ptr<VertexShaderImpl> WglRenderContext::makeVertexShaderImpl(
        std::string const& /*source*/)
{
    return std::make_shared<DummyVertexShader>();
}

std::shared_ptr<FragmentShaderImpl> WglRenderContext::makeFragmentShaderImpl(
        std::string const& /*source*/)
{
    return std::make_shared<DummyFragmentShader>();
}

std::shared_ptr<VertexBufferImpl> WglRenderContext::makeVertexBufferImpl(
        Buffer const& /*buffer*/, Usage /*usage*/)
{
    return std::make_shared<DummyVertexBuffer>();
}

std::shared_ptr<IndexBufferImpl> WglRenderContext::makeIndexBufferImpl(
        Buffer const& /*buffer*/, Usage /*usage*/)
{
    return std::make_shared<DummyIndexBuffer>();
}

std::shared_ptr<UniformBufferImpl> WglRenderContext::makeUniformBufferImpl(
        Buffer /*buffer*/, Usage /*usage*/)
{
    return std::make_shared<DummyUniformBuffer>();
}

std::shared_ptr<TextureImpl> WglRenderContext::makeTextureImpl(
        Vector2i const& /*size*/, Format /*format*/,
        Buffer const& /*buffer*/)
{
    return std::make_shared<DummyTexture>();
}

std::shared_ptr<FramebufferImpl> WglRenderContext::makeFramebufferImpl()
{
    return std::make_shared<DummyFramebuffer>();
}

std::shared_ptr<PipelineImpl> WglRenderContext::makePipeline(
        Program program,
        VertexSpec /*spec*/)
{
    return std::make_shared<DummyPipeline>(std::move(program));
}

std::shared_ptr<PipelineImpl> WglRenderContext::makePipelineWithBlend(
        Program program,
        VertexSpec /*spec*/,
        BlendMode /*srcFactor*/,
        BlendMode /*dstFactor*/)
{
    return std::make_shared<DummyPipeline>(std::move(program));
}

std::shared_ptr<UniformSetImpl> WglRenderContext::makeUniformSetImpl()
{
    return std::make_shared<DummyUniformSet>();
}

#endif

} // namespace ase


