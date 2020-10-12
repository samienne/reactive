#include "windowsplatform.h"

#include "windowswindow.h"
#include "windowsrendercontext.h"

#include "rendercontext.h"
#include "window.h"
#include "platform.h"

#include <windows.h>

namespace ase
{

Platform makeDefaultPlatform()
{
    return Platform(std::make_shared<WindowsPlatform>());
}

Window WindowsPlatform::makeWindow(Vector2i /*size*/)
{
    return Window(std::make_shared<WindowsWindow>());
}

void WindowsPlatform::handleEvents()
{
    MSG msg;
    BOOL bRet;

    if ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
    {
        if (bRet == -1)
        {
        }
        else
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

RenderContext WindowsPlatform::makeRenderContext()
{
    return RenderContext(std::make_shared<WindowsRenderContext>());
}

} // namespace ase

