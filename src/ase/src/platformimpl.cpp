#include "platformimpl.h"

#include "windowbase.h"
#include "window.h"

#include <btl/runloop.h>

#include <chrono>

namespace ase
{

void PlatformImpl::run(std::function<bool(Frame const&)> frameCallback)
{
    RunConfig config = runConfig();

    auto const step = config.frameStep;
    auto const maxFrames = config.maxFrames;
    auto const maxFps = config.maxFps;

    std::chrono::steady_clock clock;
    auto startTime = clock.now();
    auto lastFrame = startTime;
    auto nextFrame = startTime + step;

    // Time accumulated by manual steps; each step advances it by its own dt so a
    // paused driver stepping frame-by-frame gets a reproducible clock.
    std::chrono::microseconds steppedTime{ 0 };

    // Frames pumped so far this run(); only consulted with a frame budget.
    std::uint64_t framesRun = 0;

    bool tickScheduled = false;

    std::function<void(btl::RunLoop::Controller&)> tick;

    // Per dirty window: gate on the window's own backpressure (acquire blocks
    // while too many of its frames are in flight), render and present it into the
    // target it acquires, then fence it on its own queue. Backpressure is a
    // per-window property, so there is no shared frames-in-flight count and the
    // loop submits no fence itself. The render list is re-fetched each call
    // because windows open and close during a run. The headless dummy registers
    // its windows here too, so this drives their frame(); only the draw and
    // present are no-ops on the dummy queue.
    auto renderDirtyWindows = [this](Frame const& frame)
    {
        auto& renderWindows = getRenderWindows();

        for (auto& weakWindow : renderWindows)
        {
            if (auto window = weakWindow.lock())
            {
                if (window->needsRedraw())
                {
                    // acquire() gates on the window's backpressure and reports
                    // its surface state. A non-Ok status means no surface to
                    // render into -- a lost or not-yet-recreated swapchain on a
                    // backend where that can happen; tolerate it by skipping this
                    // window's frame rather than drawing into nothing. GL's
                    // surface is effectively immortal so this never skips today,
                    // but consuming the status keeps the loop from assuming so.
                    if (window->acquire() != PresentStatus::Ok)
                        continue;

                    window->frame(frame);
                    window->submitFrameFence();
                }
            }
        }
    };

    // Whether any window still wants drawing -- the on-demand re-arm condition.
    // Re-fetches the render list each call for the same reason as above.
    auto anyWindowNeedsRedraw = [this]() -> bool
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
    };

    // Post the tick at the pace the backend asked for. Uncapped -- the on-screen
    // default, and headless without an fps cap -- posts it straight away; a cap
    // defers a too-soon tick by the remaining interval.
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
            &scheduleTick, &renderDirtyWindows, &anyWindowNeedsRedraw, &tick](
            btl::RunLoop::Controller& controller)
    {
        tickScheduled = false;

        // Paused: a pause token drives frames through step() instead, so the
        // auto cadence produces nothing and does not re-arm. Events keep pumping
        // (the wake source still fires); only frame production is suspended.
        if (pauseCount_ != 0)
            return;

        // A frame budget stops the run once it is spent (headless).
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

        // Headless self-pump: keep ticking (paced) until the budget is spent,
        // with no OS source to wake the loop between frames.
        if (maxFrames != 0)
        {
            scheduleTick(controller);
            return;
        }

        // On-demand re-arm: a window that still wants drawing keeps the loop
        // ticking; acquire() upstream, not a re-arm here, is what paces a window
        // whose frames are backed up on the GPU.
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

    // Let requestFrame(), which cannot see this loop-local tick, schedule one
    // while the loop is active; cleared before run() returns.
    scheduleTick_ = [&scheduleTick](btl::RunLoop::Controller& c)
        { scheduleTick(c); };

    // A pause token's step() produces one frame off the same callback and render
    // path an auto tick takes, on the caller's dt; cleared before run() returns.
    stepFrame_ = [&steppedTime, &frameCallback, &renderDirtyWindows](
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
            // Wake on platform input; the callback drains events (firing the
            // window handlers) and schedules a tick so the frame callback runs
            // and any armed window redraws. A headless backend has no such
            // source and advances purely on requestFrame()/self-pump.
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

    // No queue.finish() here: each window owns its queue and its in-flight
    // fences now, and the caller (App) drains them in teardown before releasing
    // the windows. The loop holds no context to finish.
}

void PlatformImpl::pauseFrames()
{
    ++pauseCount_;
}

void PlatformImpl::resumeFrames()
{
    if (pauseCount_ > 0)
        --pauseCount_;

    // Resuming re-arms the auto cadence: wake the loop so a tick runs and, if a
    // window still wants drawing, keeps the on-demand loop going again.
    if (pauseCount_ == 0)
        requestFrame();
}

bool PlatformImpl::stepFrame(std::chrono::microseconds dt)
{
    return stepFrame_ ? stepFrame_(dt) : false;
}

} // namespace ase
