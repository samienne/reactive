#include "glrendercontext.h"

#include "glplatform.h"
#include "glprogram.h"
#include "glfragmentshader.h"
#include "glvertexshader.h"
#include "glvertexbuffer.h"
#include "glindexbuffer.h"
#include "gluniformbuffer.h"
#include "gltexture.h"
#include "glrenderbuffer.h"
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
    mainQueue_(std::make_shared<GlRenderQueue>(std::move(fgContext),
                [this](Dispatched d, Window& w)
                {
                    present(d, w);
                })),
    transferQueue_(platform.isBackgroundQueueEnabled()
            ? std::make_shared<GlRenderQueue>(std::move(bgContext), [](Dispatched, Window&) {})
            : mainQueue_),
    objectManager_(*this),
    defaultFramebuffer_(*this, nullptr),
    sharedFramebuffer_(*this)
{
    dispatch([bgEnabled=platform.isBackgroundQueueEnabled()](GlFunctions const&)
    {
        std::cout << "GlVendor: " << glGetString(GL_VENDOR) << std::endl;
        std::cout << "GlRenderer: " << glGetString(GL_RENDERER) << std::endl;
        std::cout << "GlVersion: " << glGetString(GL_VERSION) << std::endl;
        std::cout << "BackgroundQueueEnabled: "
            << (bgEnabled ? "yes" : "no") << std::endl;
    });
}

GlRenderContext::~GlRenderContext()
{
    mainQueue_->flush();
    transferQueue_->flush();
}

GlRenderQueue& GlRenderContext::getMainGlRenderQueue()
{
    return *mainQueue_;
}

GlRenderQueue const& GlRenderContext::getMainGlRenderQueue() const
{
    return *mainQueue_;
}

void GlRenderContext::validate(std::string_view msg) const
{
    validate(isValidationEnabled(), msg);
}

void GlRenderContext::validate(bool enabled, std::string_view msg)
{
    if (!enabled)
        return;

    auto err = glGetError();
    if (err != GL_NO_ERROR)
    {
        std::cout << "Error: " << msg << ": " << glErrorToString(err)
            << std::endl;
        glGetError();
    }
}

bool GlRenderContext::isValidationEnabled() const
{
    return true;
}

std::shared_ptr<RenderQueueImpl> GlRenderContext::getMainRenderQueue()
{
    return mainQueue_;
}

std::shared_ptr<RenderQueueImpl> GlRenderContext::getTransferQueue()
{
    return transferQueue_;
}

GlFramebuffer const& GlRenderContext::getDefaultFramebuffer() const
{
    return defaultFramebuffer_;
}

void GlRenderContext::dispatch(std::function<void(GlFunctions const&)> f)
{
    mainQueue_->dispatch(std::move(f));
}

void GlRenderContext::dispatchBg(std::function<void(GlFunctions const&)> f)
{
    transferQueue_->dispatch(std::move(f));
}

void GlRenderContext::wait() const
{
    mainQueue_->flush();
}

void GlRenderContext::waitBg() const
{
    transferQueue_->flush();
}

GlFramebuffer& GlRenderContext::getSharedFramebuffer(Dispatched)
{
    return sharedFramebuffer_;
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

std::shared_ptr<VertexBufferImpl> GlRenderContext::makeVertexBufferImpl()
{
    return objectManager_.makeVertexBuffer();
}

std::shared_ptr<IndexBufferImpl> GlRenderContext::makeIndexBufferImpl()
{
    return objectManager_.makeIndexBuffer();
}

std::shared_ptr<UniformBufferImpl> GlRenderContext::makeUniformBufferImpl()
{
    return objectManager_.makeUniformBuffer();
}

std::shared_ptr<TextureImpl> GlRenderContext::makeTextureImpl(
        Vector2i const& size, Format format)
{
    return objectManager_.makeTexture(size, format);
}

std::shared_ptr<RenderbufferImpl> GlRenderContext::makeRenderbufferImpl(
        Vector2i const& size, Format format)
{
    return objectManager_.makeRenderbuffer(size, format);
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

} // namespace

