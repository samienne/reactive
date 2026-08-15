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

bool WglPlatform::isBackgroundQueueEnabled() const
{
    return false;
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
    PIXELFORMATDESCRIPTOR pfd = {};

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

Window WglPlatform::makeOffscreenWindow(RenderContext& context, Vector2i size)
{
    auto window = std::make_shared<OffscreenWindow>(context, size);
    renderWindows_.push_back(window);
    return Window(std::move(window));
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
    HGLRC fgContext = createRawContext(3, 0);
    HGLRC bgContext = createRawContext(3, 0);

    wglShareLists(bgContext, fgContext);

    return RenderContext(std::make_shared<WglRenderContext>(*this,
                fgContext, bgContext));
}

void WglPlatform::run(RenderContext& renderContext,
        std::function<bool(Frame const&)> frameCallback)
{
    DBG("Starting WglPlatform::run");

    std::chrono::steady_clock clock;
    auto startTime = clock.now();
    auto lastFrame = startTime;
    auto nextFrame = startTime + std::chrono::microseconds(16667);

    auto framesInFlight = std::make_shared<int>(0);
    auto mainQueue = renderContext.getMainRenderQueue();

    bool tickScheduled = false;

    std::function<void(btl::RunLoop::Controller&)> tick;

    auto scheduleTick = [&tickScheduled, &tick](btl::RunLoop::Controller& controller)
    {
        if (tickScheduled)
            return;
        tickScheduled = true;
        controller.post(tick);
    };

    tick = [this, &tickScheduled, &clock, &startTime, &lastFrame, &frameCallback,
            &framesInFlight, &mainQueue, &nextFrame, &tick](
            btl::RunLoop::Controller& controller)
    {
        tickScheduled = false;

        auto thisFrame = clock.now();
        auto time = std::chrono::duration_cast<std::chrono::microseconds>(
                thisFrame - startTime);
        auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
                thisFrame - lastFrame);

        Frame frame { time, dt };

        if (!frameCallback(frame))
        {
            controller.stop();
            return;
        }

        // Skip producing a new frame while the GPU still has earlier ones in
        // flight; a fence completion frees a slot and a re-arm picks it up.
        if (*framesInFlight < 2)
        {
            for (auto& weakWindow : renderWindows_)
            {
                if (auto window = weakWindow.lock())
                {
                    if (window->needsRedraw())
                        window->frame(frame);
                }
            }

            ase::CommandBuffer commandBuffer;
            ++*framesInFlight;
            commandBuffer.pushFence([this, framesInFlight]
                {
                    runLoop().post([framesInFlight](btl::RunLoop::Controller&)
                        { --*framesInFlight; });
                });
            mainQueue.submit(std::move(commandBuffer));
        }

        auto now = clock.now();
        nextFrame += std::chrono::microseconds(16667);
        while (nextFrame < now)
            nextFrame += std::chrono::microseconds(16667);

        lastFrame = thisFrame;

        bool armed = *framesInFlight >= 2;
        for (auto& weakWindow : renderWindows_)
        {
            if (auto window = weakWindow.lock())
            {
                if (window->needsRedraw())
                {
                    armed = true;
                    break;
                }
            }
        }

        if (armed && !tickScheduled)
        {
            tickScheduled = true;
            auto delay = std::chrono::duration_cast<std::chrono::microseconds>(
                    nextFrame - clock.now());
            if (delay.count() < 0)
                delay = std::chrono::microseconds(0);
            controller.addTimer(delay, tick).detach();
        }
    };

    btl::RunLoop::Source msgSource;

    scheduleTick_ = [&scheduleTick](btl::RunLoop::Controller& c) { scheduleTick(c); };

    runLoop().post([this, &msgSource, &scheduleTick](
            btl::RunLoop::Controller& controller)
        {
            // Wake on Windows input; the callback drains the queue (firing the
            // window event callbacks) and schedules a tick so frameCallback
            // (running/sync) runs and any armed window redraws.
            msgSource = controller.addReadable(btl::fromMessageQueue(),
                    [this, &scheduleTick](btl::RunLoop::Controller& c)
                    {
                        handleEvents();
                        scheduleTick(c);
                    });

            scheduleTick(controller);
        });

    runLoop().run();

    scheduleTick_ = nullptr;

    mainQueue.finish();

    DBG("Shutting down WglPlatform...");
}

} // namespace ase

