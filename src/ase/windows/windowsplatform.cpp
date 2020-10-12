#include "windowsplatform.h"

#include "windowswindow.h"
#include "windowsrendercontext.h"

#include "rendercontext.h"
#include "window.h"
#include "platform.h"

#include <windows.h>

#include <iostream>

namespace ase
{

LRESULT CALLBACK wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_PAINT:
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

WindowsPlatform::WindowsPlatform()
{
    WNDCLASS wc = {};

    HINSTANCE hInst = GetModuleHandle(NULL);

    wc.lpfnWndProc = (WNDPROC)wndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "MainWindowClass";

    RegisterClass(&wc);
}

Platform makeDefaultPlatform()
{
    return Platform(std::make_shared<WindowsPlatform>());
}

Window WindowsPlatform::makeWindow(Vector2i size)
{
    return Window(std::make_shared<WindowsWindow>(size));
}

void WindowsPlatform::handleEvents()
{
    MSG msg;
    BOOL bRet = false;

    while (bRet == PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0)
    {
        if (bRet == -1)
            throw std::runtime_error("Error");

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

RenderContext WindowsPlatform::makeRenderContext()
{
    return RenderContext(std::make_shared<WindowsRenderContext>());
}

} // namespace ase

