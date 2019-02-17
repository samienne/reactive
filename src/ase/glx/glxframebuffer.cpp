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

void GlxFramebuffer::makeCurrent(Dispatched dispatched,
        GlRenderContext& context, GlFunctions const& /*gl*/) const
{
    auto& glxRenderContext = reinterpret_cast<GlxRenderContext&>(context);
    GlxDispatchedContext const& glxContext = glxRenderContext.getGlxContext();

    window_.makeCurrent(platform_.lockX(), glxContext.getGlxContext());

    glxRenderContext.setViewport(Dispatched(), window_.getSize());

    if (dirty_)
    {
        glClearColor(0.0, 0.0, 0.0, 1.0);

        glxRenderContext.clear(dispatched,
                GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        dirty_ = false;
    }
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

void GlxFramebuffer::clear()
{
    dirty_ = true;
}

} // namespace ase

