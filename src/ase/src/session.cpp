#include "session.h"

#include "platformimpl.h"
#include "windowimpl.h"
#include "window.h"

#include <btl/runloop.h>

namespace ase
{

Session::Session(PlatformImpl& platform,
        std::vector<std::weak_ptr<WindowImpl>>& renderWindows,
        Config config) :
    platform_(platform),
    renderWindows_(renderWindows),
    config_(std::move(config))
{
}

void Session::run(std::function<bool(Frame const&)> frameCallback)
{
    auto const step = config_.frameStep;
    auto const maxFrames = config_.maxFrames;
    auto const maxFps = config_.maxFps;

    std::chrono::steady_clock clock;
    auto startTime = clock.now();
    auto lastFrame = startTime;
    auto nextFrame = startTime + step;

    btl::RunLoop& loop = platform_.runLoop();

    // Frames pumped so far this run(); only consulted with a frame budget.
    std::uint64_t framesRun = 0;

    bool tickScheduled = false;

    std::function<void(btl::RunLoop::Controller&)> tick;

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
            &scheduleTick, &tick](btl::RunLoop::Controller& controller)
    {
        tickScheduled = false;

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

        // Per dirty window: gate on the window's own backpressure (acquire
        // blocks while too many of its frames are in flight), render and present
        // it into the target it acquires, then fence it on its own queue.
        // Backpressure is a per-window property, so there is no shared
        // frames-in-flight count here and the Session submits no fence itself.
        // With no drawable surfaces (headless dummy) renderWindows_ is empty and
        // the loop advances purely on the frame callback and the self-pump.
        for (auto& weakWindow : renderWindows_)
        {
            if (auto window = weakWindow.lock())
            {
                if (window->needsRedraw())
                {
                    window->acquire();
                    window->frame(frame);
                    window->submitFrameFence();
                }
            }
        }

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
        bool armed = false;
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

    // Let requestFrame(), which cannot see this loop-local tick, schedule one
    // while the loop is active; cleared before run() returns.
    platform_.scheduleTick_ = [&scheduleTick](btl::RunLoop::Controller& c)
        { scheduleTick(c); };

    btl::RunLoop::Source wakeSource;

    loop.post([this, &wakeSource, &scheduleTick](
            btl::RunLoop::Controller& controller)
        {
            // Wake on platform input; the callback drains events (firing the
            // window handlers) and schedules a tick so the frame callback runs
            // and any armed window redraws. A headless backend has no such
            // source and advances purely on requestFrame()/self-pump.
            if (config_.wakeSource.valid())
            {
                wakeSource = controller.addReadable(config_.wakeSource,
                        [this, &scheduleTick](btl::RunLoop::Controller& c)
                        {
                            config_.handleEvents();
                            scheduleTick(c);
                        });
            }

            scheduleTick(controller);
        });

    loop.run();

    platform_.scheduleTick_ = nullptr;

    // No queue.finish() here: each window owns its queue and its in-flight
    // fences now, and the caller (App) drains them in teardown before releasing
    // the windows. The Session holds no context to finish.
}

} // namespace ase
