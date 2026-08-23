#include "platformbase.h"

#include "windowbase.h"
#include "window.h"

#include <btl/runloop.h>

#include <tracy/Tracy.hpp>

#include <chrono>

namespace ase
{

PlatformBase::PlatformBase(btl::RunLoop& loop) :
    loop_(loop)
{
}

void PlatformBase::run(std::function<bool(Frame const&)> frameCallback)
{
    RunConfig config = runConfig();

    auto const step = config.frameStep;
    auto const maxFrames = config.maxFrames;

    std::chrono::steady_clock clock;
    auto lastFrame = clock.now();

    std::chrono::microseconds accumulator{ 0 };
    std::chrono::microseconds frameTime{ 0 };

    std::chrono::microseconds steppedTime{ 0 };
    std::uint64_t framesRun = 0;

    bool tickScheduled = false;

    std::function<void(btl::RunLoop::Controller&)> tick;

    // Run a tick as soon as possible. The app wants to re-evaluate now -- at
    // startup, on a content change, an event, or a window close -- independent
    // of any window's render cadence, so this is what drives the loop to a stop
    // when there is nothing (or nothing left) to draw.
    auto wakeTick = [&tickScheduled, &tick](btl::RunLoop::Controller& controller)
    {
        if (tickScheduled)
            return;
        tickScheduled = true;
        controller.post(tick);
    };

    auto scheduleTick = [this, &tickScheduled, &tick, &clock](
            btl::RunLoop::Controller& controller)
    {
        if (tickScheduled)
            return;

        // Wake when the earliest window that can render is next due. A
        // saturated or quiesced window schedules nothing -- its fence-wake or a
        // requestFrame() brings it back.
        auto earliest = earliestFrameTime();
        if (!earliest)
            return;

        tickScheduled = true;

        auto now = clock.now();
        if (*earliest <= now)
            controller.post(tick);
        else
            controller.addTimer(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        *earliest - now),
                    tick).detach();
    };

    tick = [this, &tickScheduled, &wakeTick, &clock, &lastFrame,
            &frameCallback, &accumulator, &frameTime, step, &framesRun,
            maxFrames, &scheduleTick](
            btl::RunLoop::Controller& controller)
    {
        tickScheduled = false;

        if (pauseCount_ != 0)
            return;

        if (maxFrames != 0 && framesRun >= maxFrames)
        {
            controller.stop();
            return;
        }

        ZoneScopedN("frameTick");

        auto thisFrame = clock.now();
        auto realElapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                thisFrame - lastFrame);
        lastFrame = thisFrame;

        accumulator += realElapsed;
        auto steps = accumulator / step;
        auto n = steps < 1 ? decltype(steps){ 1 } : steps;
        auto dt = n * step;
        accumulator -= steps * step;  // carry sub-step remainder, not n*step
        frameTime += dt;

        Frame frame { frameTime, dt };

        bool keepRunning;
        {
            ZoneScopedN("frameCallback");
            keepRunning = frameCallback(frame);
        }

        if (!keepRunning)
        {
            controller.stop();
            return;
        }

        renderDirtyWindows(frame);

        ++framesRun;

        if (maxFrames != 0)
        {
            // Headless self-pump: pump toward the budget as fast as the GPU
            // allows, ignoring per-window cadence; a saturated queue waits for a
            // fence to free a slot and wake the loop.
            if (!anyWindowSaturated())
                wakeTick(controller);
            return;
        }

        // Interactive: reschedule to the earliest window's next-frame time; a
        // saturated window instead resumes on its fence-wake, so the loop is
        // never blocked or spun on backpressure.
        scheduleTick(controller);
    };

    scheduleTick_ = [&wakeTick](btl::RunLoop::Controller& c)
        {
            wakeTick(c);
        };

    stepFrame_ = [this, &steppedTime, &frameCallback](
            std::chrono::microseconds dt) -> bool
    {
        steppedTime += dt;

        Frame frame { steppedTime, dt };

        bool keepRunning = frameCallback(frame);
        renderDirtyWindows(frame);

        return keepRunning;
    };

    btl::RunLoop::Source wakeSourceRegistration;

    loop_.post([this, &wakeSourceRegistration, &wakeTick](
            btl::RunLoop::Controller& controller)
        {
            btl::NativeHandle handle = wakeSource();
            if (handle.valid())
            {
                wakeSourceRegistration = controller.addReadable(handle,
                        [this, &wakeTick](btl::RunLoop::Controller& c)
                        {
                            handleEvents();
                            wakeTick(c);
                        });
            }

            wakeTick(controller);
        });

    loop_.run();

    scheduleTick_ = nullptr;
    stepFrame_ = nullptr;
}

void PlatformBase::renderDirtyWindows(Frame const& frame)
{
    ZoneScopedN("renderDirtyWindows");

    auto now = std::chrono::steady_clock::now();
    auto& renderWindows = getRenderWindows();

    for (auto& weakWindow : renderWindows)
    {
        if (auto window = weakWindow.lock())
        {
            // Render a window that is due and has a free in-flight slot; a
            // saturated one is skipped without blocking, and its fence wakes the
            // loop through requestFrame() to retry it.
            auto due = window->nextFrameTime();
            if (due && *due <= now && window->canAcquire())
            {
                window->frame(frame);
                window->submitFrameFence([this] { requestFrame(); });
            }
        }
    }

    // One Tracy frame per loop tick, uniform across backends. The GL windows'
    // swap runs later on the render thread and no longer marks frames itself.
    FrameMark;
}

std::optional<std::chrono::steady_clock::time_point>
PlatformBase::earliestFrameTime()
{
    std::optional<std::chrono::steady_clock::time_point> earliest;

    for (auto& weakWindow : getRenderWindows())
    {
        if (auto window = weakWindow.lock())
        {
            // Only a window that can render now sets the cadence; a saturated
            // one waits for its fence, not a timer.
            if (window->canAcquire())
                if (auto due = window->nextFrameTime())
                    if (!earliest || *due < *earliest)
                        earliest = due;
        }
    }

    return earliest;
}

bool PlatformBase::anyWindowSaturated()
{
    auto& renderWindows = getRenderWindows();

    for (auto& weakWindow : renderWindows)
    {
        if (auto window = weakWindow.lock())
        {
            if (!window->canAcquire())
                return true;
        }
    }

    return false;
}

void PlatformBase::pauseFrames()
{
    ++pauseCount_;
}

void PlatformBase::resumeFrames()
{
    if (pauseCount_ > 0)
        --pauseCount_;

    if (pauseCount_ == 0)
        requestFrame();
}

bool PlatformBase::stepFrame(std::chrono::microseconds dt)
{
    return stepFrame_ ? stepFrame_(dt) : false;
}

void PlatformBase::requestFrame()
{
    if (!wakePosted_.exchange(true))
    {
        loop_.post([this](btl::RunLoop::Controller& controller)
            {
                wakePosted_ = false;
                if (scheduleTick_)
                    scheduleTick_(controller);
            });
    }
}

btl::RunLoop& PlatformBase::runLoop()
{
    return loop_;
}

PlatformBase::RunConfig PlatformBase::runConfig()
{
    return RunConfig{};
}

btl::NativeHandle PlatformBase::wakeSource()
{
    return {};
}

} // namespace ase
