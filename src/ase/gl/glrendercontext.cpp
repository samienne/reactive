#include "glrendercontext.h"

#include "glplatform.h"
#include "glprogram.h"
#include "glfragmentshader.h"
#include "glvertexshader.h"
#include "glvertexbuffer.h"
#include "glindexbuffer.h"
#include "gluniformbuffer.h"
#include "gltexture.h"
#include "glerror.h"
#include "gltype.h"
#include "glblendmode.h"
#include "glpipeline.h"
#include "gluniformset.h"
#include "gldispatchedcontext.h"
#include "glfunctions.h"

#include "commandbuffer.h"
#include "rendercommand.h"
#include "vertexshader.h"
#include "fragmentshader.h"
#include "buffer.h"
#include "debug.h"

#include "systemgl.h"

#include <algorithm>
#include <functional>

namespace ase
{

GlRenderContext::GlRenderContext(
        GlPlatform& platform,
        std::shared_ptr<GlDispatchedContext> fgContext,
        std::shared_ptr<GlDispatchedContext> bgContext
        ) :
    platform_(platform),
    fgContext_(std::move(fgContext)),
    bgContext_(std::move(bgContext)),
    objectManager_(*this),
    renderState_(*this, *fgContext_),
    defaultFramebuffer_(*this, nullptr),
    sharedFramebuffer_(*this)
{
    dispatch([](GlFunctions const&)
    {
        std::cout << "GlVendor: " << glGetString(GL_VENDOR) << std::endl;
        std::cout << "GlRenderer: " << glGetString(GL_RENDERER) << std::endl;
        std::cout << "GlVersion: " << glGetString(GL_VERSION) << std::endl;
    });
}

GlRenderContext::~GlRenderContext()
{
    fgContext_->wait();
    bgContext_->wait();
}

void GlRenderContext::submit(CommandBuffer&& commands)
{
    renderState_.submit(std::move(commands));
}

void GlRenderContext::flush()
{
    wait();
}

void GlRenderContext::finish()
{
    dispatch([](GlFunctions const&)
            {
                glFinish();
            });
    wait();
}

void GlRenderContext::present(Window& /*window*/)
{
    renderState_.endFrame();
}

GlFramebuffer const& GlRenderContext::getDefaultFramebuffer() const
{
    return defaultFramebuffer_;
}

void GlRenderContext::dispatch(std::function<void(GlFunctions const&)>&& func)
{
    fgContext_->dispatch(std::move(func));
}

void GlRenderContext::dispatchBg(std::function<void(GlFunctions const&)>&& func)
{
    bgContext_->dispatch(std::move(func));
}

void GlRenderContext::wait() const
{
    fgContext_->wait();
}

void GlRenderContext::waitBg() const
{
    bgContext_->wait();
}

GlFramebuffer& GlRenderContext::getSharedFramebuffer(Dispatched)
{
    return sharedFramebuffer_;
}

void GlRenderContext::setViewport(Dispatched d, Vector2i size)
{
    renderState_.setViewport(d, size);
}

void GlRenderContext::clear(Dispatched d, GLbitfield mask)
{
    renderState_.clear(d, mask);
}

std::shared_ptr<ProgramImpl> GlRenderContext::makeProgramImpl(
        VertexShader const& vertexShader,
        FragmentShader const& fragmentShader)
{
    return objectManager_.makeProgram(vertexShader, fragmentShader);
}

std::shared_ptr<VertexShaderImpl> GlRenderContext::makeVertexShaderImpl(
        std::string const& source)
{
    return objectManager_.makeVertexShader(source);
}

std::shared_ptr<FragmentShaderImpl> GlRenderContext::makeFragmentShaderImpl(
        std::string const& source)
{
    return objectManager_.makeFragmentShader(source);
}

std::shared_ptr<VertexBufferImpl> GlRenderContext::makeVertexBufferImpl(
        Buffer const& buffer, Usage usage)
{
    return objectManager_.makeVertexBuffer(std::move(buffer), usage);
}

std::shared_ptr<IndexBufferImpl> GlRenderContext::makeIndexBufferImpl(
        Buffer const& buffer, Usage usage)
{
    return objectManager_.makeIndexBuffer(std::move(buffer), usage);
}

std::shared_ptr<UniformBufferImpl> GlRenderContext::makeUniformBufferImpl(
        Buffer buffer, Usage usage)
{
    return objectManager_.makeUniformBuffer(std::move(buffer), usage);
}

std::shared_ptr<TextureImpl> GlRenderContext::makeTextureImpl(
        Vector2i const& size, Format format, Buffer const& buffer)
{
    return objectManager_.makeTexture(size, format, std::move(buffer));
}

std::shared_ptr<FramebufferImpl> GlRenderContext::makeFramebufferImpl()
{
    return objectManager_.makeFramebuffer();
}

std::shared_ptr<PipelineImpl> GlRenderContext::makePipeline(
        Program program,
        VertexSpec spec)
{
    return objectManager_.makePipeline(std::move(program), std::move(spec));
}

std::shared_ptr<PipelineImpl> GlRenderContext::makePipelineWithBlend(
        Program program,
        VertexSpec spec,
        BlendMode srcFactor,
        BlendMode dstFactor)
{
    return objectManager_.makePipelineWithBlend(std::move(program),
            std::move(spec), srcFactor, dstFactor);
}

std::shared_ptr<UniformSetImpl> GlRenderContext::makeUniformSetImpl()
{
    return objectManager_.makeUniformSet();
}

GlDispatchedContext const& GlRenderContext::getFgContext() const
{
    return *fgContext_;
}

GlDispatchedContext const& GlRenderContext::getBgContext() const
{
    return *bgContext_;
}

} // namespace

