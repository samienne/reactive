#include "windowsapp.h"

#include "rendering.h"
#include "window.h"
#include "send.h"
#include "debug.h"

#include "signal/updateresult.h"
#include "signal/input.h"

#include <ase/renderqueue.h>
#include <ase/rendercontext.h>
#include <ase/wglplatform.h>
#include <ase/wglwindow.h>

#include <pmr/statistics_resource.h>
#include <pmr/unsynchronized_pool_resource.h>
#include <pmr/new_delete_resource.h>

#include <windows.h>

#include <iostream>
#include <chrono>

namespace
{
LRESULT CALLBACK wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
        break;

    default:
        break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

uint64_t s_frameId(0);

uint64_t getNextFrameId()
{
    return ++s_frameId;
}

uint64_t getCurrentFrameId()
{
    return s_frameId;
}

} // anonymous namespace

namespace reactive
{

class WglWindowGlue
{
public:
    WglWindowGlue(ase::WglPlatform& platform, ase::RenderContext&& context,
            Window window, avg::Painter painter):
        memoryPool_(pmr::new_delete_resource()),
        memoryStatistics_(&memoryPool_),
        memory_(&memoryStatistics_),
        wglWindow(platform, ase::Vector2i(800, 600)),
        context_(std::move(context)),
        window_(std::move(window)),
        painter_(std::move(painter)),
        size_(signal::input(ase::Vector2f(800, 600))),
        widget_(window_.getWidget()(
                    signal::constant(DrawContext(memory_)),
                    std::move(size_.signal)
                    )),
        titleSignal_(window_.getTitle().clone())
    {
    }

    WglWindowGlue(WglWindowGlue const&) = delete;
    WglWindowGlue& operator=(WglWindowGlue const&) = delete;

    virtual ~WglWindowGlue()
    {
        std::cout << "Maximum concurrent allocations: " <<
            memoryStatistics_.maximum_concurrent_bytes_allocated() << std::endl;
    }

    btl::option<signal::signal_time_t> frame(std::chrono::microseconds dt)
    {
        return btl::none;
    }

    uint64_t getFrames() const
    {
        return frames_;
    }

    std::string getTitle() const
    {
        return window_.getTitle().evaluate();
    }

    Widget const& getWidget() const
    {
        return widget_;
    }

private:
    pmr::unsynchronized_pool_resource memoryPool_;
    pmr::statistics_resource memoryStatistics_;
    pmr::memory_resource* memory_;
    ase::WglWindow wglWindow;
    ase::RenderContext context_;
    Window window_;
    avg::Painter painter_;
    signal::Input<ase::Vector2f> size_;
    Widget widget_;
    AnySignal<std::string> titleSignal_;
    //RenderCache cache_;
    bool resized_ = true;
    bool redraw_ = true;
    std::unordered_map<unsigned int, std::vector<InputArea>> areas_;
    std::unordered_map<ase::KeyCode,
        std::function<void(ase::KeyEvent const&)>> keys_;
    btl::option<signal::InputHandle<bool>> currentHandle_;
    btl::option<KeyboardInput::Handler> currentHandler_;
    uint64_t frames_ = 0;
    uint32_t pointerEventsOnThisFrame_ = 0;
    btl::option<InputArea> currentHoverArea_;
};

WindowsApp::WindowsApp()
{
}

void WindowsApp::addWindows(std::vector<Window> windows)
{
    for (auto&& w : windows)
        windows_.push_back(std::move(w));
}

int WindowsApp::run(AnySignal<bool> running) &&
{
    ase::WglPlatform platform;

    std::vector<btl::shared<WglWindowGlue>> glues;
    glues.reserve(windows_.size());

    for (auto&& w : windows_)
    {
        ase::RenderContext context(platform);
        avg::Painter painter(context);

        glues.push_back(std::make_shared<WglWindowGlue>(platform,
                    std::move(context), std::move(w), painter));
    }

    std::chrono::steady_clock clock;
    auto startTime = clock.now();
    auto lastFrame = startTime;

    DBG("Reactive Wgl backend running...");

    while (running.evaluate())
    {
        auto thisFrame = clock.now();
        auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
                thisFrame - lastFrame);

        //auto events = platform.getEvents();

        reactive::signal::FrameInfo frame{getNextFrameId(), dt};
        auto timeToNext = running.updateBegin(frame);
        auto timeToNext2 = running.updateEnd(frame);
        timeToNext = reactive::signal::min(timeToNext, timeToNext2);

        for (auto& glue : glues)
        {
            //auto t = glue->frame(events, dt);

            timeToNext = reactive::signal::min(timeToNext, t);
        }

        if (timeToNext.valid())
        {
            auto frameTime = std::chrono::duration_cast<
                std::chrono::microseconds>(clock.now() - thisFrame);
            auto remaining = *timeToNext - frameTime;
            if (remaining.count() > 0)
                std::this_thread::sleep_for(remaining);
        }

        lastFrame = thisFrame;
    }

    DBG("Shutting down...");

    auto endTime = clock.now();
    std::chrono::duration<double> time = endTime - startTime;

    for (auto const& glue : glues)
    {
        DBG("Window \"%1\" had FPS of %2.", glue->getTitle(),
                (double)glue->getFrames() / time.count());
    }

    return 0;

    HINSTANCE hInst = GetModuleHandle(nullptr);

    WNDCLASS wc = {};

    //wc.style = 0;
    wc.lpfnWndProc = (WNDPROC)wndProc;
    //wc.cbClsExtra = 0;
    wc.hInstance = hInst;
    //wc.hIcon = LoadIcon((HINSTANCE)nullptr, IDI_APPLICATION);
    //wc.hCursor = LoadCursor((HINSTANCE)nullptr, IDC_ARROW);
    //wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    //wc.lpszMenuName = "MainMenu";
    wc.lpszClassName = "WindowClass";

    if (!RegisterClass(&wc))
        throw std::runtime_error("Unable to register window class");

    HWND hWnd = 0;
    hWnd = CreateWindowEx(0, "WindowClass", "Sample", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            (HWND)nullptr, (HMENU)nullptr, hInst, (LPVOID)nullptr);

    if (!hWnd)
        throw std::runtime_error("Unable to create a window.");

    ShowWindow(hWnd, SW_SHOW);

    MSG msg;
    BOOL ret;

    while ((ret = GetMessage(&msg, nullptr, 0, 0)) != 0)
    {
        if (ret == -1)
            throw std::runtime_error("error in message loop");

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    //MessageBox( 0, "Press OK", "Hi", MB_SETFOREGROUND );

    return msg.wParam;
}

int WindowsApp::run() &&
{
    auto running = signal::input(true);
    for (auto&& w : windows_)
        w = std::move(*w).onClose(send(false, running.handle));

    return std::move(*this).run(std::move(running.signal));
}

std::unique_ptr<AppImpl> makeAppImpl()
{
    return std::make_unique<WindowsApp>();
}

} // namespace reactive
