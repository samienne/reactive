#include "dummyplatform.h"

#include "dummywindow.h"
#include "dummyrendercontext.h"

#include "rendercontext.h"
#include "window.h"
#include "platform.h"

namespace ase
{

Platform makeDefaultPlatform()
{
    return Platform(std::make_shared<DummyPlatform>());
}

Window DummyPlatform::makeWindow(Vector2i /*size*/)
{
    return Window(std::make_shared<DummyWindow>());
}

void DummyPlatform::handleEvents()
{
}

RenderContext DummyPlatform::makeRenderContext()
{
    return RenderContext(std::make_shared<DummyRenderContext>());
}

void DummyPlatform::run(RenderContext&,
        std::function<bool(Frame const&)> frameCallback)
{
    Frame frame{};

    // A tick is "scheduled" while one is posted; this loop-thread-only flag
    // dedupes so ticks never stack.
    bool tickScheduled = false;

    std::function<void(btl::RunLoop::Controller&)> tick;

    auto scheduleTick = [&](btl::RunLoop::Controller& controller)
    {
        if (tickScheduled)
            return;
        tickScheduled = true;
        controller.post(tick);
    };

    // Headless: a tick only runs the frame callback (there is nothing to
    // render). It does not re-post; the loop blocks until requestFrame() wakes
    // it, so any sources registered on it (a remote socket, a timer) are
    // serviced meanwhile.
    tick = [&](btl::RunLoop::Controller& controller)
    {
        tickScheduled = false;

        if (!frameCallback(frame))
        {
            controller.stop();
            return;
        }
    };

    runLoop().post([&](btl::RunLoop::Controller& controller)
        {
            scheduleTick_ = [&](btl::RunLoop::Controller& c) { scheduleTick(c); };
            scheduleTick(controller);
        });
    runLoop().run();

    scheduleTick_ = nullptr;
}

void DummyPlatform::requestFrame()
{
    // May be called off the loop thread (e.g. an async signal completing), so
    // wake through a thread-safe post; the atomic coalesces a burst into one.
    if (!wakePosted_.exchange(true))
    {
        runLoop().post([this](btl::RunLoop::Controller& controller)
            {
                wakePosted_ = false;
                if (scheduleTick_)
                    scheduleTick_(controller);
            });
    }
}

} // namespace ase

