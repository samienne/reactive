#include "glframebuffer.h"

#include "gltexture.h"
#include "glrenderbuffer.h"
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
    framebuffer_(rhs.framebuffer_),
    colorTargets_(std::move(rhs.colorTargets_)),
    depthTarget_(std::move(rhs.depthTarget_)),
    size_(rhs.size_),
    isDefault_(rhs.isDefault_),
    dirty_(rhs.dirty_)
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

void GlFramebuffer::makeCurrent(Dispatched d, GlDispatchedContext&,
        GlRenderState& renderState, GlFunctions const& gl) const
{
    if (isDefault_)
    {
        gl.glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        return;
    }

    if (!framebuffer_)
        gl.glGenFramebuffers(1, &framebuffer_);

    gl.glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

    if (dirty_)
    {
        dirty_ = false;

        int index = 0;
        for (std::optional<Texture> const& target : colorTargets_)
        {
            if (target.has_value())
            {
                GlTexture& texture = const_cast<GlTexture&>(
                        target->getImpl<GlTexture>());
                texture.allocate(d, gl);

                gl.glFramebufferTexture2D(GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_2D,
                        texture.getGlObject(), 0);
            }

            ++index;
        }

        if (depthTarget_.has_value())
        {
            GlRenderbuffer& depth = const_cast<GlRenderbuffer&>(
                    depthTarget_->getImpl<GlRenderbuffer>());
            depth.makeCurrent(d, gl);

            gl.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                    GL_RENDERBUFFER, depth.getGlObject());
        }
    }

    // A window framebuffer sizes the viewport to its drawable; an offscreen one
    // has no drawable, so it must size the viewport to its target here.
    renderState.setViewport(d, size_);
}

void GlFramebuffer::setColorTarget(size_t index, Texture texture)
{
    size_ = texture.getImpl<GlTexture>().getSize();
    colorTargets_.at(index) = std::move(texture);

    dirty_ = true;
}

void GlFramebuffer::setColorTarget(size_t /*index*/, Renderbuffer /*texture*/)
{
    assert(false);
}

void GlFramebuffer::unsetColorTarget(size_t index)
{
    colorTargets_.at(index) = std::nullopt;

    dirty_ = true;
}

void GlFramebuffer::setDepthTarget(Renderbuffer buffer)
{
    depthTarget_.emplace(std::move(buffer));

    dirty_ = true;
}

void GlFramebuffer::unsetDepthTarget()
{
    depthTarget_.reset();

    dirty_ = true;
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

