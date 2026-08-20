#include "platformbase.h"

#include "windowbase.h"
#include "window.h"

#include <btl/runloop.h>

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
    auto const maxFps = config.maxFps;

    std::chrono::steady_clock clock;
    auto startTime = clock.now();
    auto lastFrame = startTime;
    auto nextFrame = startTime + step;

    std::chrono::microseconds steppedTime{ 0 };
    std::uint64_t framesRun = 0;

    bool tickScheduled = false;

    std::function<void(btl::RunLoop::Controller&)> tick;

    auto scheduleTick = [&tickScheduled, &tick, &clock, &lastFrame, maxFps](
            btl::RunLoop::Controller& controller)
    {
        if (tickScheduled)
            return;
        tickScheduled = true;

        if (maxFps == 0)
        {
            controller.post(tick);
            return;
        }

        auto interval = std::chrono::microseconds(1000000 / maxFps);
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                clock.now() - lastFrame);
        if (elapsed >= interval)
            controller.post(tick);
        else
            controller.addTimer(interval - elapsed, tick).detach();
    };

    tick = [this, &tickScheduled, &clock, &startTime, &lastFrame,
            &frameCallback, &nextFrame, step, &framesRun, maxFrames,
            &scheduleTick, &tick](
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

        renderDirtyWindows(frame);

        auto now = clock.now();
        nextFrame += step;
        while (nextFrame < now)
            nextFrame += step;

        lastFrame = thisFrame;
        ++framesRun;

        if (maxFrames != 0)
        {
            scheduleTick(controller);
            return;
        }

        if (anyWindowNeedsRedraw() && !tickScheduled)
        {
            tickScheduled = true;
            auto delay = std::chrono::duration_cast<std::chrono::microseconds>(
                    nextFrame - clock.now());
            if (delay.count() < 0)
                delay = std::chrono::microseconds(0);
            controller.addTimer(delay, tick).detach();
        }
    };

    scheduleTick_ = [&scheduleTick](btl::RunLoop::Controller& c)
        {
            scheduleTick(c);
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

    loop_.post([this, &wakeSourceRegistration, &scheduleTick](
            btl::RunLoop::Controller& controller)
        {
            btl::NativeHandle handle = wakeSource();
            if (handle.valid())
            {
                wakeSourceRegistration = controller.addReadable(handle,
                        [this, &scheduleTick](btl::RunLoop::Controller& c)
                        {
                            handleEvents();
                            scheduleTick(c);
                        });
            }

            scheduleTick(controller);
        });

    loop_.run();

    scheduleTick_ = nullptr;
    stepFrame_ = nullptr;
}

void PlatformBase::renderDirtyWindows(Frame const& frame)
{
    auto& renderWindows = getRenderWindows();

    for (auto& weakWindow : renderWindows)
    {
        if (auto window = weakWindow.lock())
        {
            if (window->needsRedraw())
            {
                if (window->acquire() != PresentStatus::Ok)
                    continue;

                window->frame(frame);
                window->submitFrameFence();
            }
        }
    }
}

bool PlatformBase::anyWindowNeedsRedraw()
{
    auto& renderWindows = getRenderWindows();

    for (auto& weakWindow : renderWindows)
    {
        if (auto window = weakWindow.lock())
        {
            if (window->needsRedraw())
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
