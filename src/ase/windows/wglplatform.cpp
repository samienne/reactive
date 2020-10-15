#include "wglplatform.h"

#include "wglwindow.h"
#include "wglrendercontext.h"

#include "rendercontext.h"
#include "window.h"
#include "platform.h"

#include <windows.h>
#include <GL/gl.h>
#include <GL/wglext.h>

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
            std::cout << "destroy: " << hwnd << std::endl;
            PostQuitMessage(0);
            break;

        case WM_CLOSE:
            std::cout << "close" << std::endl;
            break;

        case WM_QUIT:
            std::cout << "quit" << std::endl;
            return 0;
            break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

HWND createDummyWindow()
{
    WNDCLASS wc = {};

    HINSTANCE hInst = GetModuleHandle(NULL);

    wc.lpfnWndProc = (WNDPROC)DefWindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "DummyWindowClass";
    wc.style = CS_OWNDC;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
            0,
            "DummyWindowClass",
            "DummyWindow",
            WS_DISABLED,
            CW_USEDEFAULT, CW_USEDEFAULT,
            1, 1,
            NULL, // parent
            NULL, // Menu
            hInst,
            NULL // Additional application data
            );

    if (hwnd == 0)
        throw std::runtime_error("Unable to create window");

    return hwnd;
}

HGLRC createDummyContext(PIXELFORMATDESCRIPTOR pfd, HWND dummyWindow)
{
    HDC dummyDc = GetDC(dummyWindow);
    int dummyPixelFormat = ChoosePixelFormat(dummyDc, &pfd);
    SetPixelFormat(dummyDc, dummyPixelFormat, &pfd);

    HGLRC dummyContext = wglCreateContext(dummyDc);

    return dummyContext;
}

PFNWGLCREATECONTEXTATTRIBSARBPROC getWglCreateContextAttribsARB(
        HWND window, HGLRC context)
{
    HDC dc = GetDC(window);

    wglMakeCurrent(dc, context);

    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC) wglGetProcAddress(
                "wglCreateContextAttribsARB");

    wglMakeCurrent(NULL, NULL);

    if (!wglCreateContextAttribsARB)
        throw std::runtime_error("Unable to get wglCreateContextAttribsARB.");

    return wglCreateContextAttribsARB;
}


WglPlatform::WglPlatform()
{
    try
    {
        dummyWindow_ = createDummyWindow();
        dummyDc_ = GetDC(dummyWindow_);
        dummyContext_ = createDummyContext(getPixelFormatDescriptor(),
                dummyWindow_);
        wglCreateContextAttribsARB_ = getWglCreateContextAttribsARB(
                dummyWindow_, dummyContext_);
    }
    catch(std::exception& e)
    {
        std::cout << e.what() << std::endl;
        throw;
    }

    WNDCLASS wc = {};

    HINSTANCE hInst = GetModuleHandle(NULL);

    wc.lpfnWndProc = (WNDPROC)wndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "MainWindowClass";

    RegisterClass(&wc);
}

WglPlatform::~WglPlatform()
{
    wglMakeCurrent(nullptr, nullptr);
    if (dummyContext_)
        wglDeleteContext(dummyContext_);

    if (dummyWindow_)
        DestroyWindow(dummyWindow_);
}

Platform makeDefaultPlatform()
{
    return Platform(std::make_shared<WglPlatform>());
}

HGLRC WglPlatform::createRawContext(int minor, int major)
{
    static const int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, minor,
        WGL_CONTEXT_MINOR_VERSION_ARB, major,
        NULL
    };

    wglMakeCurrent(dummyDc_, dummyContext_);
    HGLRC context = wglCreateContextAttribsARB_(dummyDc_, NULL, attribs);
    wglMakeCurrent(nullptr, nullptr);

    if (!context)
    {
        std::cout << getLastErrorString() << std::endl;
        throw std::runtime_error("Unable to create context");
    }

    return context;
}

HDC WglPlatform::getDummyDc() const
{
    return dummyDc_;
}

PIXELFORMATDESCRIPTOR WglPlatform::getPixelFormatDescriptor() const
{
    PIXELFORMATDESCRIPTOR pfd = {0};

    pfd.nSize = sizeof(pfd);
    pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    return pfd;
}

std::string WglPlatform::getLastErrorString()
{
    DWORD errorMessageId = ::GetLastError();
    if (!errorMessageId)
        return std::string();

    LPSTR msgBuffer = nullptr;
    size_t size = FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            errorMessageId,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&msgBuffer, 0,
            nullptr);

    std::string msg(msgBuffer, size);

    LocalFree(msgBuffer);

    return msg;
}

Window WglPlatform::makeWindow(Vector2i size)
{
    try
    {
    return Window(std::make_shared<WglWindow>(*this, size));
    }
    catch(std::exception& e)
    {
        std::cout << "error:" << e.what() << std::endl;
        throw;
    }
}

void WglPlatform::handleEvents()
{
    MSG msg;

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0)
    {
        //std::cout << msg.message << std::endl;
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (msg.message == WM_QUIT)
            break;
    }
}

RenderContext WglPlatform::makeRenderContext()
{
    return RenderContext(std::make_shared<WglRenderContext>(*this));
}

} // namespace ase

