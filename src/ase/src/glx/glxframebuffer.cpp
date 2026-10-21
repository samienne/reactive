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

    renderState.setViewport(dispatched, window_.getScaledSize());
}

void GlxFramebuffer::setColorTarget(size_t /*index*/, Texture /*texture*/)
{
    assert(false);
}

void GlxFramebuffer::setColorTarget(size_t /*index*/, Renderbuffer /*texture*/)
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

void GlxFramebuffer::setStencilTarget(Renderbuffer /*buffer*/)
{
    assert(false);
}

void GlxFramebuffer::unsetStencilTarget()
{
    assert(false);
}

} // namespace ase

