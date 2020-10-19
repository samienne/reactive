#include "wglplatform.h"

#include "wglwindow.h"
#include "wglrendercontext.h"

#include "rendercontext.h"
#include "window.h"
#include "platform.h"

#include <windows.h>
#include <GL/gl.h>
#include <GL/wglext.h>

#include <shellscalingapi.h>

#include <unordered_map>
#include <iostream>

namespace ase
{

std::unordered_map<HWND, std::weak_ptr<WglWindow>> windows;

LRESULT CALLBACK wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto i = windows.find(hwnd);
    if (i != windows.end())
    {
        if (auto p = i->second.lock())
            return p->handleWindowsEvent(hwnd, uMsg, wParam, lParam);
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
        HDC dc, HGLRC context)
{
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
        SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

        dummyWindow_ = createDummyWindow();
        if (!dummyWindow_)
            throw std::runtime_error("Unable to create a dummy window.");

        dummyDc_ = GetDC(dummyWindow_);
        if (!dummyDc_)
            throw std::runtime_error("No dummy DC available.");

        dummyContext_ = createDummyContext(getPixelFormatDescriptor(),
                dummyWindow_);
        if (!dummyContext_)
            throw std::runtime_error("Unable to create a dummy context");

        wglCreateContextAttribsARB_ = getWglCreateContextAttribsARB(
                dummyDc_, dummyContext_);
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
    wc.style = CS_OWNDC;

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

    if (!context)
    {
        auto error = getLastErrorString();
        wglMakeCurrent(nullptr, nullptr);
        std::cout << error << std::endl;
        throw std::runtime_error("Unable to create context: " + error);
    }

    wglMakeCurrent(nullptr, nullptr);

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
        auto wglWindow = std::make_shared<WglWindow>(*this, size, 1.0f);
        windows.insert(std::make_pair(wglWindow->getHwnd(), wglWindow));
        return Window(std::move(wglWindow));
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

    // Drop expired weak pointers to windows
    auto i = windows.begin();
    while (i != windows.end())
    {
        if (i->second.expired())
            i = windows.erase(i);
        else
            ++i;
    }
}

RenderContext WglPlatform::makeRenderContext()
{
    HGLRC fgContext = createRawContext(3, 0);
    HGLRC bgContext = createRawContext(3, 0);

    wglShareLists(bgContext, fgContext);

    return RenderContext(std::make_shared<WglRenderContext>(*this,
                fgContext, bgContext));
}

} // namespace ase

