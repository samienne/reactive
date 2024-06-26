#include "glxrendercontext.h"

#include "glxwindow.h"
#include "glxplatform.h"
#include "glxdispatchedcontext.h"

#include <GL/glx.h>

#include <tracy/Tracy.hpp>

namespace ase
{


GlxRenderContext::GlxRenderContext(
        GlxPlatform& platform,
        std::shared_ptr<GlxDispatchedContext>&& fgContext,
        std::shared_ptr<GlxDispatchedContext>&& bgContext
        ) :
    GlRenderContext(platform, std::move(fgContext), std::move(bgContext)),
    platform_(platform)
{
}

GlxRenderContext::GlxRenderContext(GlxPlatform& platform) :
    GlxRenderContext(platform,
            std::make_shared<GlxDispatchedContext>(platform),
            std::make_shared<GlxDispatchedContext>(platform)
            )
{
}

GlxRenderContext::~GlxRenderContext()
{
    // make sure queues are clear before calling parent destructor
    wait();
    waitBg();
}

void GlxRenderContext::present(Dispatched d, Window& window)
{
    ZoneScopedN("GlxRenderContext::present");
    GlxWindow& glxWindow = window.getImpl<GlxWindow>();
    glxWindow.present(d);
}

} // namespace ase

