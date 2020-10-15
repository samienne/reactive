#include "wglrendercontext.h"

#include "wglwindow.h"
#include "wgldispatchedcontext.h"
#include "wglplatform.h"

#include "window.h"

#undef min

#include <iostream>

namespace ase
{

WglRenderContext::WglRenderContext(WglPlatform& platform) :
    GlRenderContext(platform,
            std::make_shared<WglDispatchedContext>(platform),
            std::make_shared<WglDispatchedContext>(platform)
            ),
    platform_(platform)
{
    wglShareLists(
            static_cast<WglDispatchedContext const&>(getFgContext()).getWglContext(),
            static_cast<WglDispatchedContext const&>(getBgContext()).getWglContext()
            );
}

void WglRenderContext::present(Window& window)
{
    window.getImpl<WglWindow>().present();
}

} // namespace ase


