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

GlxFramebuffer::GlxFramebuffer(GlxPlatform& platform, GlxWindow& window) :
    platform_(platform),
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

    window_.makeCurrent(platform_.lockX(), glxContext.getGlxContext());

    Vector2i size(
            window_.getSize()[0] * window_.getScalingFactor(),
            window_.getSize()[1] * window_.getScalingFactor()
            );

    renderState.setViewport(dispatched, size);
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

