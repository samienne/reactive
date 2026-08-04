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

void DummyPlatform::setMaxFps(unsigned int fps)
{
    maxFps_ = fps;
}

void DummyPlatform::run(RenderContext&,
        std::function<bool(Frame const&)> frameCallback)
{
    std::chrono::steady_clock clock;
    auto startTime = clock.now();
    auto lastFrame = startTime;

    bool tickScheduled = false;

    std::function<void(btl::RunLoop::Controller&)> tick;

    // Uncapped, a due tick is posted straight away and the loop stays purely
    // on-demand. With a cap, a tick that comes due sooner than the frame
    // interval is deferred by a one-shot timer for the remainder instead.
    auto scheduleTick = [this, &tickScheduled, &tick, &clock, &lastFrame](
            btl::RunLoop::Controller& controller)
    {
        if (tickScheduled)
            return;
        tickScheduled = true;

        if (maxFps_ == 0)
        {
            controller.post(tick);
            return;
        }

        auto interval = std::chrono::microseconds(1000000 / maxFps_);
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                clock.now() - lastFrame);
        if (elapsed >= interval)
            controller.post(tick);
        else
            controller.addTimer(interval - elapsed, tick).detach();
    };

    // Headless: a tick runs the frame callback with the elapsed time (there is
    // nothing to render). It does not re-post; the loop blocks until
    // requestFrame() wakes it, so any sources registered on it (a remote socket,
    // a timer) are serviced meanwhile.
    tick = [&tickScheduled, &clock, &startTime, &lastFrame, &frameCallback](
            btl::RunLoop::Controller& controller)
    {
        tickScheduled = false;

        auto thisFrame = clock.now();
        auto time = std::chrono::duration_cast<std::chrono::microseconds>(
                thisFrame - startTime);
        auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
                thisFrame - lastFrame);

        Frame frame { time, dt };
        lastFrame = thisFrame;

        if (!frameCallback(frame))
        {
            controller.stop();
            return;
        }
    };

    scheduleTick_ = [&scheduleTick](btl::RunLoop::Controller& c) { scheduleTick(c); };

    runLoop().post([&scheduleTick](btl::RunLoop::Controller& controller)
        {
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

