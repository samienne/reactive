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
    return reinterpret_cast<WglDispatchedContext const&>(
            getFgContext()
            );
}

void WglRenderContext::present(Window& window)
{
    dispatch([this, &window](GlFunctions const& gl) mutable
            {
                WglWindow& wglWindow = window.getImpl<WglWindow>();
                WglDispatchedContext const& wglContext =
                    static_cast<WglDispatchedContext const&>(getFgContext());

                wglMakeCurrent(
                        wglWindow.getDc(),
                        wglContext.getWglContext()
                        );

                //glClearColor(0.0, 0.0, 1.0, 1.0);
                //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                SwapBuffers(wglWindow.getDc());

                //window.getImpl<WglWindow>().present();
            });

    wait();
}

} // namespace ase


