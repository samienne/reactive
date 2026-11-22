#include "platformbase.h"

#include "windowbase.h"
#include "window.h"

#include <btl/runloop.h>

#include <tracy/Tracy.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <optional>

namespace ase
{

namespace
{
    constexpr std::chrono::microseconds kFenceRecheckInterval{ 1000 };
} // namespace

PlatformBase::PlatformBase(btl::RunLoop& loop) :
    loop_(loop)
{
}

void PlatformBase::run(std::function<bool(Frame const&)> frameCallback)
{
    RunConfig config = runConfig();

    // Cadence frozen at startup; also read by earliestFrameTime().
    frameStep_ = config.frameStep;
    scheduledWaitEnd_ = std::nullopt;

    std::chrono::steady_clock clock;
    auto lastFrame = clock.now();

    std::chrono::microseconds frameTime{ 0 };

    std::chrono::microseconds steppedTime{ 0 };

    bool tickScheduled = false;

    // The pending tick's timer and wall-clock due time; unset while the
    // pending tick is a post.
    btl::RunLoop::Timer tickTimer;
    std::optional<std::chrono::steady_clock::time_point> tickTimerDue;

    std::function<void(btl::RunLoop::Controller&)> tick;

    // Nothing wakes the loop when a fence signals, so while a window is held
    // back by its fences a timer polls them and wakes the loop, as a
    // requestFrame() would, once one of those windows gets a slot back.
    std::size_t heldBack = 0;
    btl::RunLoop::Timer fenceTimer;
    bool fenceTimerArmed = false;
    std::function<void(btl::RunLoop::Controller&)> recheckFences;

    auto armFenceRecheck = [this, &heldBack, &fenceTimer, &fenceTimerArmed,
            &recheckFences](btl::RunLoop::Controller& controller)
    {
        if (heldBack == 0 || fenceTimerArmed)
            return;

        fenceTimerArmed = true;
        fenceTimer = controller.addTimer(kFenceRecheckInterval,
                recheckFences);
    };

    // A wake cuts a pending timer short, but never to sooner than frameStep
    // after the last tick.
    auto wakeTick = [this, &tickScheduled, &tickTimer, &tickTimerDue, &tick,
            &clock, &lastFrame](btl::RunLoop::Controller& controller)
    {
        if (tickScheduled && !tickTimerDue)
            return;

        auto now = clock.now();

        if (tickScheduled)
        {
            auto paced = std::max(now, lastFrame + frameStep_);
            if (*tickTimerDue <= paced)
                return;

            tickTimer = controller.addTimer(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        paced - now),
                    tick);
            tickTimerDue = paced;
            return;
        }

        tickScheduled = true;
        controller.post(tick);
    };

    // Between ticks the count only grows through a requestFrame(), which
    // wakes the loop itself, so a lower count means a window got a slot back.
    // A fence can signal between any two counts, so each recheck counts once
    // and keeps that count as the next baseline.
    recheckFences = [this, &heldBack, &fenceTimer, &fenceTimerArmed,
            &armFenceRecheck, &wakeTick](btl::RunLoop::Controller& controller)
    {
        fenceTimerArmed = false;
        fenceTimer.detach();

        auto count = countHeldBackWindows();
        if (count < heldBack)
            wakeTick(controller);

        heldBack = count;
        armFenceRecheck(controller);
    };

    auto scheduleTick = [this, &tickScheduled, &tickTimer, &tickTimerDue, &tick,
            &clock, &frameTime, &heldBack, &armFenceRecheck](
            btl::RunLoop::Controller& controller)
    {
        // A window whose fence signals after this count is a drop the recheck
        // sees; one that signalled before it is renderable again and sets the
        // earliest frame time below.
        heldBack = countHeldBackWindows();
        armFenceRecheck(controller);

        if (tickScheduled)
            return;

        // Wake when the earliest window that can render is next due, in the
        // loop's quantized frame time. A saturated or quiesced window schedules
        // nothing -- the fence recheck or a requestFrame() brings it back.
        auto earliest = earliestFrameTime();
        if (!earliest)
            return;

        tickScheduled = true;

        if (*earliest <= frameTime)
        {
            controller.post(tick);
        }
        else
        {
            auto delay = *earliest - frameTime;
            tickTimer = controller.addTimer(delay, tick);
            tickTimerDue = clock.now() + delay;
            scheduledWaitEnd_ = tickTimerDue;
        }
    };

    tick = [this, &tickScheduled, &tickTimer, &tickTimerDue, &clock,
            &lastFrame, &frameCallback, &frameTime, &scheduleTick](
            btl::RunLoop::Controller& controller)
    {
        tickScheduled = false;
        tickTimer.detach();
        tickTimerDue = std::nullopt;

        if (pauseCount_ != 0)
            return;

        auto waited = scheduledWaitEnd_
            ? std::chrono::duration_cast<std::chrono::microseconds>(
                    *scheduledWaitEnd_ - lastFrame)
            : std::chrono::microseconds(0);
        scheduledWaitEnd_ = std::nullopt;

        ZoneScopedN("frameTick");

        auto thisFrame = clock.now();
        auto realElapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                thisFrame - lastFrame);
        lastFrame = thisFrame;

        // dt is the real elapsed time capped at the longer of the scheduled
        // wait and four frame steps; the wait counts in full even when a wake
        // ends it early.
        auto const maxDt = std::max(4 * frameStep_, waited);
        auto dt = realElapsed > maxDt ? maxDt : realElapsed;

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

        // Reschedule to the earliest window's next-frame time; a saturated
        // window instead resumes on the fence recheck, so the loop is never
        // blocked on backpressure.
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

    auto& renderWindows = getRenderWindows();

    for (auto& weakWindow : renderWindows)
    {
        if (auto window = weakWindow.lock())
        {
            // Render a window that is due and has a free in-flight slot; a
            // saturated one is skipped without blocking, and the fence recheck
            // wakes the loop to retry it.
            auto due = window->nextFrameTime();
            if (due && *due <= frame.time && window->canAcquire())
            {
                window->frame(frame);
                window->submitFrameFence();
            }
        }
    }

    FrameMark;
}

std::optional<std::chrono::microseconds>
PlatformBase::earliestFrameTime()
{
    std::optional<std::chrono::microseconds> earliest;

    for (auto& weakWindow : getRenderWindows())
    {
        if (auto window = weakWindow.lock())
        {
            // Only a window that can render now sets the cadence; a saturated
            // one waits for the fence recheck, not a timer.
            if (window->canAcquire())
            {
                auto due = window->nextFrameTime();
                if (due)
                {
                    // A live animation asks to render ASAP (onFrame returns 0);
                    // the platform, not the content, owns the pace, so hold the
                    // next frame to at least frameStep after the last one rather
                    // than free-running the loop. A longer requested delay
                    // (an idle window) is left alone.
                    auto paced = window->lastFrameTime() + frameStep_;
                    if (paced > *due)
                        due = paced;

                    if (!earliest || *due < *earliest)
                        earliest = due;
                }
            }
        }
    }

    return earliest;
}

std::size_t PlatformBase::countHeldBackWindows()
{
    std::size_t count = 0;

    for (auto& weakWindow : getRenderWindows())
    {
        auto window = weakWindow.lock();
        if (window && window->nextFrameTime() && !window->canAcquire())
            ++count;
    }

    return count;
}

void PlatformBase::pauseFrames()
{
    ++pauseCount_;

    auto const now = std::chrono::steady_clock::now();
    if (scheduledWaitEnd_ && *scheduledWaitEnd_ > now)
        scheduledWaitEnd_ = now;
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
