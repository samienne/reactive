#include "globjectmanager.h"

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
#include "glframebuffer.h"
#include "glrendercontext.h"
#include "gluniformset.h"
#include "glrenderbuffer.h"

#include "usage.h"
#include "vertexshader.h"
#include "fragmentshader.h"
#include "buffer.h"

namespace ase
{

GlObjectManager::GlObjectManager(GlRenderContext& context) :
    context_(context)
{
}

std::shared_ptr<GlProgram> GlObjectManager::makeProgram(
        VertexShader const& vertexShader,
        FragmentShader const& fragmentShader)
{
    return std::make_shared<GlProgram>(context_,
            vertexShader.getImpl<GlVertexShader>(),
            fragmentShader.getImpl<GlFragmentShader>()
            );
}

std::shared_ptr<GlVertexShader> GlObjectManager::makeVertexShader(
        std::string const& source)
{
    return std::make_shared<GlVertexShader>(context_, source);
}

std::shared_ptr<GlFragmentShader> GlObjectManager::makeFragmentShader(
        std::string const& source)
{
    return std::make_shared<GlFragmentShader>(context_, source);
}

std::shared_ptr<GlVertexBuffer> GlObjectManager::makeVertexBuffer(
        Buffer const& buffer, Usage usage)
{
    auto vb = std::make_shared<GlVertexBuffer>(context_);

    if (usage == Usage::StreamDraw || usage == Usage::StreamRead
            || usage == Usage::StreamCopy)
    {
        context_.dispatch([&vb, ownBuffer=std::move(buffer), usage]
                (GlFunctions const& gl) mutable
                {
                    vb->setData(Dispatched(), gl, std::move(ownBuffer), usage);
                });
        context_.wait();
    }
    else
    {
        context_.dispatchBg([&vb, ownBuffer=std::move(buffer), usage]
                (GlFunctions const& gl) mutable
                {
                    vb->setData(Dispatched(), gl, std::move(ownBuffer), usage);
                    glFlush();
                });
        context_.waitBg();
    }


    return vb;
}

std::shared_ptr<GlIndexBuffer> GlObjectManager::makeIndexBuffer(
        Buffer const& buffer, Usage usage)
{
    auto ib = std::make_shared<GlIndexBuffer>(context_);

    context_.dispatchBg([&ib, ownBuffer=std::move(buffer), usage]
            (GlFunctions const& gl) mutable
            {
                ib->setData(Dispatched(), gl, std::move(ownBuffer), usage);
                glFlush();
            });

    context_.waitBg();

    return ib;
}

std::shared_ptr<GlUniformBuffer> GlObjectManager::makeUniformBuffer(
        Buffer const& buffer,
        Usage usage)
{
    auto ub = std::make_shared<GlUniformBuffer>(context_);

    context_.dispatchBg([&ub, ownBuffer=std::move(buffer), usage]
        (GlFunctions const& gl) mutable
        {
            ub->setData(Dispatched(), gl, std::move(ownBuffer), usage);
            glFlush();
        });

    context_.waitBg();

    return ub;
}

std::shared_ptr<GlTexture> GlObjectManager::makeTexture(
        Vector2i const& size, Format format, Buffer const& buffer)
{
    auto texture = std::make_shared<GlTexture>(context_);

    auto ownBuffer = buffer;
    auto ownSize = size;
    context_.dispatchBg([texture, ownBuffer, ownSize, format]
            (GlFunctions const& gl)
            {
                texture->setData(Dispatched(), gl, ownSize, format, ownBuffer);
                glFlush();
            });

    context_.waitBg();

    return texture;
}

std::shared_ptr<GlRenderbuffer> GlObjectManager::makeRenderbuffer(
        Vector2i const& size, Format format)
{
    return std::make_shared<GlRenderbuffer>(context_, size, format);
}

std::shared_ptr<GlFramebuffer> GlObjectManager::makeFramebuffer()
{
    return std::make_shared<GlFramebuffer>(context_);
}

std::shared_ptr<GlPipeline> GlObjectManager::makePipeline(
        Program program,
        VertexSpec spec)
{
    return std::make_shared<GlPipeline>(
            context_,
            std::move(program),
            std::move(spec)
            );
}

std::shared_ptr<GlPipeline> GlObjectManager::makePipelineWithBlend(
        Program program,
        VertexSpec spec,
        BlendMode srcFactor,
        BlendMode dstFactor)
{
    return std::make_shared<GlPipeline>(
            context_,
            std::move(program),
            std::move(spec),
            srcFactor,
            dstFactor
            );
}

std::shared_ptr<GlUniformSet> GlObjectManager::makeUniformSet()
{
    return std::make_shared<GlUniformSet>(context_);
}

} // namespace ase

