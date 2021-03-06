#include "glframebuffer.h"

#include "gltexture.h"
#include "glrendercontext.h"
#include "glplatform.h"
#include "glfunctions.h"

#include "renderbuffer.h"
#include "texture.h"

#include "debug.h"

namespace ase
{

GlFramebuffer::GlFramebuffer(GlRenderContext& context) :
    context_(context),
    framebuffer_(0)
{
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
        context_.dispatchBg([this](GlFunctions const& gl)
            {
                gl.glDeleteFramebuffers(1, &framebuffer_);
                framebuffer_ = 0;
            });

        context_.waitBg();
    }
}

void GlFramebuffer::destroy(Dispatched, GlFunctions const& gl)
{
    if (framebuffer_)
    {
        gl.glDeleteFramebuffers(1, &framebuffer_);
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

/*
void GlFramebuffer::setColorTarget(Dispatched d, GlFunctions const& gl,
        size_t index, Texture const& texture)
{
    GlTexture const& glTexture = texture.getImpl<GlTexture>();
    setColorTarget(d, gl, index, const_cast<GlTexture&>(glTexture));
}

void GlFramebuffer::setColorTarget(Dispatched, GlFunctions const& gl,
        size_t index, GlTexture const& texture)
{
    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index,
            GL_TEXTURE_2D, texture.texture_, 0);
}
*/

void GlFramebuffer::makeCurrent(Dispatched, GlDispatchedContext&,
        GlRenderState&, GlFunctions const& gl) const
{
    gl.glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

    if (!dirty_ || isDefault_)
        return;

    if (!framebuffer_)
        gl.glGenFramebuffers(1, &framebuffer_);

    dirty_ = false;

    int index = 0;
    for (Attachment const& attachment : colorAttachments_)
    {
        gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index,
                attachment.target, attachment.object, 0);

        ++index;
    }

    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_2D, depthAttachment_, 0);

    gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D, stencilAttachment_, 0);
}

void GlFramebuffer::setColorTarget(size_t index, Texture texture)
{
    colorAttachments_.at(index) = Attachment{
        GL_TEXTURE_2D,
        texture.getImpl<GlTexture>().getGlObject()
    };

    dirty_ = true;
}

void GlFramebuffer::setColorTarget(size_t /*index*/, Renderbuffer /*texture*/)
{
    assert(false);
}

void GlFramebuffer::unsetColorTarget(size_t index)
{
    colorAttachments_.at(index) = Attachment{ 0, 0 };

    dirty_ = true;
}

void GlFramebuffer::setDepthTarget(Renderbuffer /*buffer*/)
{
    assert(false);
}

void GlFramebuffer::unsetDepthTarget()
{
    assert(false);
}

void GlFramebuffer::setStencilTarget(Renderbuffer /*buffer*/)
{
    assert(false);
}

void GlFramebuffer::unsetStencilTarget()
{
    assert(false);
}

GlFramebuffer::GlFramebuffer(GlRenderContext& context, std::nullptr_t) :
    context_(context),
    framebuffer_(0),
    isDefault_(true)
{
}

} // namespace

