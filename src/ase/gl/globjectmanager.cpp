#include "globjectmanager.h"

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
#include "glrendercontext.h"

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

    auto ownBuffer = buffer;
    context_.dispatchBg([vb, ownBuffer, usage](GlFunctions const& gl)
            {
                vb->setData(Dispatched(), gl, ownBuffer, usage);
            });

    // No waiting

    return vb;
}

std::shared_ptr<GlIndexBuffer> GlObjectManager::makeIndexBuffer(
        Buffer const& buffer, Usage usage)
{
    auto ib = std::make_shared<GlIndexBuffer>(context_);

    auto ownBuffer = buffer;
    context_.dispatchBg([ib, ownBuffer, usage](GlFunctions const& gl)
            {
                ib->setData(Dispatched(), gl, ownBuffer, usage);
            });

    // No waiting

    return ib;
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
            });

    // No waiting

    return texture;
}

std::shared_ptr<GlRenderTargetObject> GlObjectManager::makeRenderTargetObject()
{
    return std::make_shared<GlRenderTargetObject>(context_);
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

} // namespace ase

