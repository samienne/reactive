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

std::shared_ptr<GlVertexBuffer> GlObjectManager::makeVertexBuffer()
{
    return std::make_shared<GlVertexBuffer>(context_);
}

std::shared_ptr<GlIndexBuffer> GlObjectManager::makeIndexBuffer()
{
    return std::make_shared<GlIndexBuffer>(context_);
}

std::shared_ptr<GlUniformBuffer> GlObjectManager::makeUniformBuffer()
{
    return std::make_shared<GlUniformBuffer>(context_);
}

std::shared_ptr<GlTexture> GlObjectManager::makeTexture(
        Vector2i const& size, Format format)
{
    return std::make_shared<GlTexture>(context_, size, format);
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

