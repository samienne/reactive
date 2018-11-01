#include "glplatform.h"

#include "glprogram.h"
#include "glvertexshader.h"
#include "glfragmentshader.h"
#include "glvertexbuffer.h"
#include "glindexbuffer.h"
#include "gltexture.h"
#include "glrendertargetobject.h"
#include "glrendercontext.h"
#include "glpipeline.h"

#include "buffer.h"

#include "debug.h"

namespace ase
{

GlPlatform::GlPlatform()
{
    DBG("GlPlatform size: %1 bytes.", sizeof(GlPlatform));
    DBG("GlProgram size: %1 bytes.", sizeof(GlProgram));
    DBG("GlVertexShader size: %1 bytes.", sizeof(GlVertexShader));
    DBG("GlFragmentShader size: %1 bytes.", sizeof(GlFragmentShader));
    DBG("GlVertexBuffer size: %1 bytes.", sizeof(GlVertexBuffer));
    DBG("GlIndexBuffer size: %1 bytes.", sizeof(GlIndexBuffer));
    DBG("GlRenderContext size: %1 bytes.", sizeof(GlRenderContext));
    DBG("GlRenderTargetObject size: %1 bytes.", sizeof(GlRenderTargetObject));
    DBG("GlTexture size: %1 bytes.", sizeof(GlTexture));
}

GlPlatform::~GlPlatform()
{
}

void GlPlatform::dispatchBackground(std::function<void()>&& func)
{
    auto& context_ = getDefaultContext().getImpl<GlRenderContext>();
    context_.dispatch(std::move(func));
}

void GlPlatform::waitBackground() const
{
    auto const& context_ = getDefaultContext().getImpl<GlRenderContext>();
    context_.wait();
}

std::shared_ptr<ProgramImpl> GlPlatform::makeProgramImpl(
        RenderContext& context, VertexShaderImpl const& vertexShader,
        FragmentShaderImpl const& fragmentShader)
{
    return std::make_shared<GlProgram>(context,
            reinterpret_cast<GlVertexShader const&>(vertexShader),
            reinterpret_cast<GlFragmentShader const&>(fragmentShader));
}

std::shared_ptr<VertexShaderImpl> GlPlatform::makeVertexShaderImpl(
        RenderContext& context, std::string const& source)
{
    return std::make_shared<GlVertexShader>(context, source);
}

std::shared_ptr<FragmentShaderImpl> GlPlatform::makeFragmentShaderImpl(
        RenderContext& context, std::string const& source)
{
    return std::make_shared<GlFragmentShader>(context, source);
}

std::shared_ptr<VertexBufferImpl> GlPlatform::makeVertexBufferImpl(
        RenderContext& context, Buffer const& buffer, Usage usage)
{
    GlRenderContext& glContext = context.getImpl<GlRenderContext>();
    auto vb = std::make_shared<GlVertexBuffer>(context);

    auto ownBuffer = buffer;
    glContext.dispatch([&glContext, vb, ownBuffer, usage]()
            {
                vb->setData(Dispatched(), glContext, ownBuffer, usage);
            });

    // No waiting

    return std::move(vb);
}

std::shared_ptr<IndexBufferImpl> GlPlatform::makeIndexBufferImpl(
        RenderContext& context, Buffer const& buffer, Usage usage)
{
    GlRenderContext& glContext = context.getImpl<GlRenderContext>();
    auto ib = std::make_shared<GlIndexBuffer>(context);

    auto ownBuffer = buffer;
    glContext.dispatch([&glContext, ib, ownBuffer, usage]()
            {
                ib->setData(Dispatched(), glContext, ownBuffer, usage);
            });

    // No waiting

    return std::move(ib);
}

std::shared_ptr<TextureImpl> GlPlatform::makeTextureImpl(
        RenderContext& context, Vector2i const& size, Format format,
        Buffer const& buffer)
{
    GlRenderContext& glContext = context.getImpl<GlRenderContext>();
    auto texture = std::make_shared<GlTexture>(context);

    auto ownBuffer = buffer;
    auto ownSize = size;
    glContext.dispatch([texture, ownBuffer, ownSize, format]()
            {
                texture->setData(Dispatched(), ownSize, format, ownBuffer);
            });

    // No waiting

    return std::move(texture);
}

std::shared_ptr<RenderTargetObjectImpl> GlPlatform::makeRenderTargetObjectImpl(
        RenderContext& context)
{
    return std::make_shared<GlRenderTargetObject>(context);
}

std::shared_ptr<PipelineImpl> GlPlatform::makePipeline(
        Program program,
        VertexSpec spec)
{
    return std::make_shared<GlPipeline>(
            std::move(program),
            std::move(spec)
            );
}

std::shared_ptr<PipelineImpl> GlPlatform::makePipelineWithBlend(
        Program program,
        VertexSpec spec,
        BlendMode srcFactor,
        BlendMode dstFactor)
{
    return std::make_shared<GlPipeline>(
            std::move(program),
            std::move(spec),
            srcFactor,
            dstFactor
            );
}

} // namespace ase

