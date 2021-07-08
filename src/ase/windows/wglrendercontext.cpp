#include "wglrendercontext.h"

#include "wglwindow.h"
#include "wgldispatchedcontext.h"
#include "wglplatform.h"

#include "window.h"

#include <GL/gl.h>
#include <GL/wglext.h>

#undef min

#include <iostream>

namespace ase
{

WglRenderContext::WglRenderContext(WglPlatform& platform, HGLRC fgContext,
        HGLRC bgContext) :
    GlRenderContext(platform,
            std::make_shared<WglDispatchedContext>(platform, fgContext),
            std::make_shared<WglDispatchedContext>(platform, bgContext)
            ),
    platform_(platform)
{
}

WglDispatchedContext const& WglRenderContext::getWglContext() const
{
    return static_cast<WglDispatchedContext const&>(
            getMainGlRenderQueue().getDispatcher()
            );
}

void WglRenderContext::present(Dispatched, Window& window)
{
    WglWindow& wglWindow = window.getImpl<WglWindow>();

    wglMakeCurrent(
            wglWindow.getDc(),
            getWglContext().getWglContext()
            );

    SwapBuffers(wglWindow.getDc());
}

} // namespace ase


