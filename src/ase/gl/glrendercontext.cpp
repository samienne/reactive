#include "glrendercontext.h"

#include "glplatform.h"
#include "glprogram.h"
#include "glfragmentshader.h"
#include "glvertexshader.h"
#include "glvertexbuffer.h"
#include "glindexbuffer.h"
#include "gltexture.h"
#include "glerror.h"
#include "gltype.h"
#include "glblendmode.h"
#include "glpipeline.h"
#include "glrendertargetobject.h"

#include "commandbuffer.h"
#include "rendercommand.h"
#include "rendertarget.h"
#include "rendertargetimpl.h"
#include "vertexshader.h"
#include "fragmentshader.h"
#include "buffer.h"
#include "debug.h"

#include <GL/gl.h>

#include <algorithm>
#include <functional>

namespace ase
{

GlRenderContext::GlRenderContext(GlPlatform& platform) :
    platform_(platform),
    dispatcher_(),
    objectManager_(*this),
    defaultFramebuffer_(*this)
{
}

GlRenderContext::~GlRenderContext()
{
}

GlPlatform& GlRenderContext::getPlatform() const
{
    return platform_;
}

void GlRenderContext::submit(CommandBuffer&& commands)
{
    dispatch([this, commands=std::move(commands)]() mutable
        {
            renderState_->submit(Dispatched(), gl_, std::move(commands));
        });
}

void GlRenderContext::flush()
{
    wait();
}

void GlRenderContext::finish()
{
    dispatch([]()
            {
                glFinish();
            });
    wait();
}

GlFunctions const& GlRenderContext::getGlFunctions() const
{
    return gl_;
}

GlFramebuffer const& GlRenderContext::getDefaultFramebuffer() const
{
    return defaultFramebuffer_;
}

void GlRenderContext::dispatch(std::function<void()>&& func)
{
    dispatcher_.run(std::move(func));
}

void GlRenderContext::dispatchBg(std::function<void()>&& func)
{
    dispatcherBg_.run(std::move(func));
}

void GlRenderContext::wait() const
{
    dispatcher_.wait();
}

void GlRenderContext::waitBg() const
{
    dispatcherBg_.wait();
}

void GlRenderContext::glInit(Dispatched d, GlFunctions const& gl)
{
    gl_ = gl;

    sharedFramebuffer_ = btl::just(GlFramebuffer(d, *this));

    renderState_ = btl::just(GlRenderState(d, *this));
}

void GlRenderContext::glDeinit(Dispatched d)
{
    if (renderState_.valid())
    {
        renderState_->deinit(d, gl_);
        renderState_ = btl::none;
    }

    sharedFramebuffer_->destroy(Dispatched(), *this);
}

GlFramebuffer& GlRenderContext::getSharedFramebuffer(Dispatched)
{
    return *sharedFramebuffer_;
}

void GlRenderContext::setViewport(Dispatched d, Vector2i size)
{
    renderState_->setViewport(d, gl_, size);
}

void GlRenderContext::clear(Dispatched d, GLbitfield mask)
{
    renderState_->clear(d, gl_, mask);
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

std::shared_ptr<TextureImpl> GlRenderContext::makeTextureImpl(
        Vector2i const& size, Format format, Buffer const& buffer)
{
    return objectManager_.makeTexture(size, format, std::move(buffer));
}

std::shared_ptr<RenderTargetObjectImpl> GlRenderContext::makeRenderTargetObjectImpl()
{
    return objectManager_.makeRenderTargetObject();
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

} // namespace

