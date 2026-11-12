// Must precede any header that pulls in windows.h: it brings in winsock2 first,
// which blocks the older winsock.h that windows.h would otherwise include.
#include <btl/win32/nativehandle_win32.h>

#include "wglplatform.h"

#include "wglwindow.h"
#include "wglrendercontext.h"
#include "offscreenwindow.h"

#include "commandbuffer.h"
#include "renderqueue.h"
#include "rendercontext.h"
#include "window.h"
#include "platform.h"
#include "debug.h"

#include "tracy/Tracy.hpp"

#include <btl/future.h>
#include <btl/runloop.h>

#include <windows.h>

#include <GL/gl.h>
#include <GL/wglext.h>

#include <shellscalingapi.h>

#include <unordered_map>
#include <iostream>
#include <queue>
#include <algorithm>

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

template <typename T>
T getWglProc(char const* name)
{
    return reinterpret_cast<T>(wglGetProcAddress(name));
}

int chooseSrgbPixelFormat(HDC dc, PFNWGLCHOOSEPIXELFORMATARBPROC choose)
{
    int const attribs[] = {
        WGL_DRAW_TO_WINDOW_ARB, TRUE,
        WGL_SUPPORT_OPENGL_ARB, TRUE,
        WGL_DOUBLE_BUFFER_ARB, TRUE,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB, 24,
        WGL_ALPHA_BITS_ARB, 8,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, TRUE,
        0
    };

    int format = 0;
    UINT count = 0;
    if (!choose(dc, attribs, nullptr, 1, &format, &count) || count == 0)
        return 0;

    return format;
}


WglPlatform::WglPlatform(btl::RunLoop& loop) :
    GlPlatform(loop)
{
    try
    {
        SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

        // A window takes a pixel format only once, so the context that
        // loads the WGL entry points gets a throwaway window.
        PIXELFORMATDESCRIPTOR const pfd = getPixelFormatDescriptor();
        HWND bootstrapWindow = createDummyWindow();
        HDC bootstrapDc = GetDC(bootstrapWindow);
        pixelFormat_ = ChoosePixelFormat(bootstrapDc, &pfd);
        HGLRC bootstrapContext = nullptr;
        if (pixelFormat_ && SetPixelFormat(bootstrapDc, pixelFormat_, &pfd))
            bootstrapContext = wglCreateContext(bootstrapDc);

        if (!bootstrapContext)
        {
            std::string const error = getLastErrorString();
            DestroyWindow(bootstrapWindow);
            throw std::runtime_error(
                    "Unable to create a bootstrap context: " + error);
        }

        wglMakeCurrent(bootstrapDc, bootstrapContext);
        wglCreateContextAttribsARB_ =
            getWglProc<PFNWGLCREATECONTEXTATTRIBSARBPROC>(
                    "wglCreateContextAttribsARB");
        auto wglChoosePixelFormatARB =
            getWglProc<PFNWGLCHOOSEPIXELFORMATARBPROC>(
                    "wglChoosePixelFormatARB");

        if (!wglChoosePixelFormatARB)
            DBG("WglPlatform: wglChoosePixelFormatARB unavailable, "
                    "keeping the legacy pixel format");
        else if (int format = chooseSrgbPixelFormat(bootstrapDc,
                    wglChoosePixelFormatARB))
            pixelFormat_ = format;
        else
            DBG("WglPlatform: no sRGB-capable pixel format, "
                    "keeping the legacy pixel format");

        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(bootstrapContext);
        DestroyWindow(bootstrapWindow);

        if (!wglCreateContextAttribsARB_)
            throw std::runtime_error("Unable to get wglCreateContextAttribsARB.");

        dummyWindow_ = createDummyWindow();
        dummyDc_ = GetDC(dummyWindow_);
        if (!dummyDc_)
            throw std::runtime_error("No dummy DC available.");

        setPixelFormat(dummyDc_);
        dummyContext_ = wglCreateContext(dummyDc_);
        if (!dummyContext_)
            throw std::runtime_error("Unable to create a dummy context");
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

Platform makeDefaultPlatform(btl::RunLoop& loop)
{
    return Platform(std::make_shared<WglPlatform>(loop));
}

bool WglPlatform::isBackgroundQueueEnabled() const
{
    return false;
}

HGLRC WglPlatform::createRawContext(int major, int minor)
{
    int const attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, major,
        WGL_CONTEXT_MINOR_VERSION_ARB, minor,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    wglMakeCurrent(dummyDc_, dummyContext_);
    HGLRC context = wglCreateContextAttribsARB_(dummyDc_, NULL, attribs);
    DWORD const error = ::GetLastError();
    wglMakeCurrent(nullptr, nullptr);

    // The caller reports a failed creation, not the reset above.
    if (!context)
        ::SetLastError(error);

    return context;
}

HGLRC WglPlatform::createContext()
{
    if (gl3Enabled_)
    {
        if (HGLRC context = createRawContext(3, 3))
            return context;

        gl3Enabled_ = false;
        DBG("WglPlatform: Failed to create OpenGl 3.3 core context: %1",
                getLastErrorString());
    }

    HGLRC context = wglCreateContext(dummyDc_);
    if (!context)
        throw std::runtime_error("Unable to create context: "
                + getLastErrorString());

    return context;
}

HDC WglPlatform::getDummyDc() const
{
    return dummyDc_;
}

PIXELFORMATDESCRIPTOR WglPlatform::getPixelFormatDescriptor() const
{
    PIXELFORMATDESCRIPTOR pfd = {};

    pfd.nSize = sizeof(pfd);
    pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    return pfd;
}

void WglPlatform::setPixelFormat(HDC dc) const
{
    PIXELFORMATDESCRIPTOR pfd = {};
    DescribePixelFormat(dc, pixelFormat_, sizeof(pfd), &pfd);

    if (!SetPixelFormat(dc, pixelFormat_, &pfd))
        throw std::runtime_error("Unable to set pixel format: "
                + getLastErrorString());
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

Window WglPlatform::makeWindow(RenderContext& context, Vector2i size,
        bool headless)
{
    if (headless)
    {
        auto window = std::make_shared<OffscreenWindow>(context, size);
        renderWindows_.push_back(window);
        return Window(std::move(window));
    }

    try
    {
        auto wglWindow = std::make_shared<WglWindow>(*this, context, size, 1.0f);
        // Assign rather than insert: Windows reuses an HWND once its window is
        // gone, and an expired entry under that handle lingers here until the
        // next pass of handleEvents(), where insert() would keep it and leave
        // the new window without a receiver for its events.
        windows[wglWindow->getHwnd()] = wglWindow;
        renderWindows_.push_back(wglWindow);
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

    renderWindows_.erase(
            std::remove_if(renderWindows_.begin(), renderWindows_.end(),
                [](auto& w) { return w.expired(); }),
            renderWindows_.end());
}

RenderContext WglPlatform::makeRenderContext()
{
    HGLRC fgContext = createContext();
    HGLRC bgContext = createContext();

    wglShareLists(bgContext, fgContext);

    return RenderContext(std::make_shared<WglRenderContext>(*this,
                fgContext, bgContext));
}

btl::NativeHandle WglPlatform::wakeSource()
{
    // Wake the loop on the thread message queue; the loop drains it through
    // handleEvents().
    return btl::fromMessageQueue();
}

std::vector<std::weak_ptr<WindowBase>>& WglPlatform::getRenderWindows()
{
    // Each window carries its own context and backpressure now, so the loop
    // drives the render list without a context of its own.
    return renderWindows_;
}

} // namespace ase

