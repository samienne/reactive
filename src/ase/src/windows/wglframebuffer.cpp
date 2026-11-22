#include "wglframebuffer.h"

#include "wglwindow.h"
#include "wglrendercontext.h"
#include "wgldispatchedcontext.h"
#include "wglplatform.h"

#include "renderbuffer.h"
#include "texture.h"
#include "dispatcher.h"

#include <iostream>

namespace ase
{

WglFramebuffer::WglFramebuffer(WglWindow& window) :
    window_(window)
{
}

void WglFramebuffer::makeCurrent(Dispatched dispatched,
        GlDispatchedContext& context, GlRenderState& renderState,
        GlFunctions const& /*gl*/) const
{
    auto& wglContext = reinterpret_cast<WglDispatchedContext&>(
            context);

    wglContext.makeCurrent(dispatched, window_);

    renderState.setViewport(dispatched, pixelSize_);
}

void WglFramebuffer::setPixelSize(Dispatched, Vector2i size)
{
    pixelSize_ = size;
}

Vector2i WglFramebuffer::getSize(Dispatched) const
{
    return pixelSize_;
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

} // namespace ase

