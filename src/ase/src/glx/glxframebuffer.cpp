#include "glxframebuffer.h"

#include "glxwindow.h"
#include "glxrendercontext.h"
#include "glxdispatchedcontext.h"
#include "glxplatform.h"

#include "renderbuffer.h"
#include "texture.h"
#include "dispatcher.h"

namespace ase
{

GlxFramebuffer::GlxFramebuffer(GlxWindow& window) :
    window_(window)
{
}

void GlxFramebuffer::makeCurrent(
        Dispatched dispatched,
        GlDispatchedContext& context,
        GlRenderState& renderState,
        GlFunctions const& /*gl*/) const
{
    auto& glxContext = reinterpret_cast<GlxDispatchedContext&>(context);

    glxContext.makeCurrent(dispatched, window_);

    renderState.setViewport(dispatched, pixelSize_);
}

void GlxFramebuffer::setPixelSize(Dispatched, Vector2i size)
{
    pixelSize_ = size;
}

Vector2i GlxFramebuffer::getSize(Dispatched) const
{
    return pixelSize_;
}

bool GlxFramebuffer::canReadback() const
{
    return false;
}

void GlxFramebuffer::setColorTarget(size_t /*index*/, Texture /*texture*/)
{
    assert(false);
}

void GlxFramebuffer::unsetColorTarget(size_t /*index*/)
{
    assert(false);
}

void GlxFramebuffer::setDepthTarget(Renderbuffer /*buffer*/)
{
    assert(false);
}

void GlxFramebuffer::unsetDepthTarget()
{
    assert(false);
}

} // namespace ase

