#include "wglframebuffer.h"

#include "wglwindow.h"
#include "wglrendercontext.h"
#include "wgldispatchedcontext.h"
#include "wglplatform.h"

#include "renderbuffer.h"
#include "texture.h"
#include "dispatcher.h"

namespace ase
{

WglFramebuffer::WglFramebuffer(WglPlatform& platform, WglWindow& window) :
    platform_(platform),
    window_(window)
{
}

void WglFramebuffer::makeCurrent(Dispatched dispatched,
        GlRenderContext& context, GlFunctions const& /*gl*/) const
{
    auto& wglRenderContext = reinterpret_cast<WglRenderContext&>(context);
    WglDispatchedContext const& wglContext = wglRenderContext.getWglContext();

    wglMakeCurrent(window_.getDc(), wglContext.getWglContext());

    Vector2i size(
            window_.getSize()[0] * window_.getScalingFactor(),
            window_.getSize()[1] * window_.getScalingFactor()
            );

    wglRenderContext.setViewport(dispatched, size);

    if (dirty_)
    {
        glClearColor(0.0, 0.0, 0.0, 1.0);

        wglRenderContext.clear(dispatched,
                GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        dirty_ = false;
    }
}

void WglFramebuffer::setColorTarget(size_t /*index*/, Texture /*texture*/)
{
    assert(false);
}

void WglFramebuffer::setColorTarget(size_t /*index*/, Renderbuffer /*texture*/)
{
    assert(false);
}

void WglFramebuffer::unsetColorTarget(size_t /*index*/)
{
    assert(false);
}

void WglFramebuffer::setDepthTarget(Renderbuffer /*buffer*/)
{
    assert(false);
}

void WglFramebuffer::unsetDepthTarget()
{
    assert(false);
}

void WglFramebuffer::setStencilTarget(Renderbuffer /*buffer*/)
{
    assert(false);
}

void WglFramebuffer::unsetStencilTarget()
{
    assert(false);
}

void WglFramebuffer::clear()
{
    dirty_ = true;
}

} // namespace ase

