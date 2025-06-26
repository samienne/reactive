#include "wglrendercontext.h"

#include "wglwindow.h"
#include "wgldispatchedcontext.h"
#include "wglplatform.h"

#include "window.h"

#include <tracy/Tracy.hpp>

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

void WglRenderContext::present(Dispatched /*dispatched*/, Window& window)
{
    ZoneScoped;
    WglWindow& wglWindow = window.getImpl<WglWindow>();

    /*if (wglWindow.getDc() != wglGetCurrentDC()
            || getWglContext().getWglContext() != wglGetCurrentContext())*/
    {
        ZoneScopedN("wglMakeCurrent");
        /*
        static_cast<WglDispatchedContext&>(
                getMainGlRenderQueue().getDispatcher()
                ).makeCurrent(dispatched, wglWindow);
        */
    }

    {
        ZoneScopedN("SwapBuffers");
        SwapBuffers(wglWindow.getDc());
        FrameMark;
    }
}

} // namespace ase


