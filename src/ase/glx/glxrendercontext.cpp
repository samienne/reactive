#include "glxrendercontext.h"

#include "glxwindow.h"
#include "glxplatform.h"
#include "glxdispatchedcontext.h"

#include "rendertarget.h"

#include "debug.h"

#include <GL/glx.h>

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
}

void GlxRenderContext::present(Window& window)
{
    dispatch([&window](GlFunctions const&)
            {
                GlxWindow& glxWindow = static_cast<GlxWindow&>(window);
                glxWindow.present(Dispatched());
            });
    wait();
}

GlxDispatchedContext const& GlxRenderContext::getGlxContext() const
{
    return reinterpret_cast<GlxDispatchedContext const&>(getFgContext());
}

} // namespace ase

