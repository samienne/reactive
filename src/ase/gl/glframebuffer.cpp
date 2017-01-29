#include "glframebuffer.h"

#include "gltexture.h"
#include "glrendercontext.h"
#include "glplatform.h"

#include "texture.h"

#include "debug.h"

namespace ase
{

GlFramebuffer::GlFramebuffer() :
    platform_(0),
    framebuffer_(0)
{
}

GlFramebuffer::GlFramebuffer(Dispatched, GlPlatform& platform,
        GlRenderContext& context) :
    platform_(&platform),
    framebuffer_(0)
{
    context.getGlFunctions().glGenFramebuffers(1, &framebuffer_);
}

GlFramebuffer::~GlFramebuffer()
{
    if (framebuffer_ && platform_)
    {
        auto& glContext = platform_->getDefaultContext()
            .getImpl<GlRenderContext>();
        platform_->dispatchBackground([this, &glContext]()
            {
                auto& gl = glContext.getGlFunctions();
                gl.glDeleteFramebuffers(1, &framebuffer_);
                framebuffer_ = 0;
            });
        platform_->waitBackground();
    }
}

void GlFramebuffer::destroy(Dispatched, GlRenderContext& context)
{
    if (framebuffer_)
    {
        context.getGlFunctions().glDeleteFramebuffers(1, &framebuffer_);
        framebuffer_ = 0;
    }
}

GlFramebuffer& GlFramebuffer::operator=(GlFramebuffer&& rhs) noexcept
{
    if (framebuffer_ && platform_)
    {
        framebuffer_ = 0;
        GLuint const framebuffer = framebuffer_;
        auto& glContext = platform_->getDefaultContext()
            .getImpl<GlRenderContext>();
        platform_->dispatchBackground([this, &glContext, framebuffer]()
            {
                glContext.getGlFunctions().glDeleteFramebuffers(1,
                    &framebuffer);
            });
    }

    framebuffer_ = rhs.framebuffer_;
    platform_ = rhs.platform_;

    rhs.framebuffer_ = 0;
    rhs.platform_ = 0;

    return *this;
}

bool GlFramebuffer::operator==(GlFramebuffer const& rhs) const
{
    return framebuffer_ == rhs.framebuffer_;
}

bool GlFramebuffer::operator!=(GlFramebuffer const& rhs) const
{
    return framebuffer_ != rhs.framebuffer_;
}

bool GlFramebuffer::operator<(GlFramebuffer const& rhs) const
{
    return framebuffer_ < rhs.framebuffer_;
}

GlFramebuffer::operator bool() const
{
    return framebuffer_;
}

void GlFramebuffer::setColorTarget(Dispatched d, GlRenderContext& context,
        size_t index, Texture const& texture)
{
    GlTexture const& glTexture = texture.getImpl<GlTexture>();
    setColorTarget(d, context, index, const_cast<GlTexture&>(glTexture));
}

void GlFramebuffer::setColorTarget(Dispatched, GlRenderContext& context,
        size_t index, GlTexture const& texture)
{
    auto& gl = context.getGlFunctions();
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index,
            GL_TEXTURE_2D, texture.texture_, 0);
}

void GlFramebuffer::makeCurrent(Dispatched, GlRenderContext& context) const
{
    context.getGlFunctions().glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
}

} // namespace

