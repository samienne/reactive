#include "glframebuffer.h"

#include "gltexture.h"
#include "glrendercontext.h"
#include "glplatform.h"

#include "texture.h"

#include "debug.h"

namespace ase
{

GlFramebuffer::GlFramebuffer(GlRenderContext& context) :
    context_(context),
    framebuffer_(0)
{
}

GlFramebuffer::GlFramebuffer(Dispatched, GlRenderContext& context) :
    context_(context),
    framebuffer_(0)
{
    context.getGlFunctions().glGenFramebuffers(1, &framebuffer_);
}

GlFramebuffer::GlFramebuffer(GlFramebuffer&& rhs) noexcept :
    context_(rhs.context_),
    framebuffer_(rhs.framebuffer_)
{
    rhs.framebuffer_ = 0;
}

GlFramebuffer::~GlFramebuffer()
{
    if (framebuffer_)
    {
        context_.dispatchBg([this]()
            {
                auto& gl = context_.getGlFunctions();
                gl.glDeleteFramebuffers(1, &framebuffer_);
                framebuffer_ = 0;
            });

        context_.waitBg();
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

void GlFramebuffer::setColorTarget(Dispatched d, size_t index,
        Texture const& texture)
{
    GlTexture const& glTexture = texture.getImpl<GlTexture>();
    setColorTarget(d, index, const_cast<GlTexture&>(glTexture));
}

void GlFramebuffer::setColorTarget(Dispatched, size_t index,
        GlTexture const& texture)
{
    auto& gl = context_.getGlFunctions();
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index,
            GL_TEXTURE_2D, texture.texture_, 0);
}

void GlFramebuffer::makeCurrent(Dispatched) const
{
    context_.getGlFunctions().glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
}

} // namespace

