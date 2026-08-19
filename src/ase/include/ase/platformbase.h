#pragma once

#include "platformimpl.h"

#include <btl/nativehandle.h>
#include <btl/runloop.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace ase
{
    class WindowBase;
    struct Frame;

    /**
     * @brief The shared base every backend platform derives from. It owns the
     * single frame-loop implementation -- `run()`, the pause/step control, and
     * `requestFrame()` over the injected run loop -- and leaves each backend only
     * its per-tick hooks: event draining, cadence config, wake source, and the
     * live render-window list.
     */
    class ASE_EXPORT PlatformBase : public PlatformImpl
    {
    public:
        /** @brief The static cadence the shared frame loop targets, plus the
         * headless self-pump parameters. Just the scalars that differ between
         * GLX, WGL and the dummy; the backend-owned resources (wake source and
         * render list) are separate virtual accessors, and the loop body itself
         * is the same for all three. */
        struct RunConfig
        {
            /** The interval the frame cadence targets. */
            std::chrono::microseconds frameStep = std::chrono::microseconds(16667);

            /** A non-zero budget bounds the run and makes it self-pump to that
             * many frames (headless); zero leaves the loop on-demand. */
            std::uint64_t maxFrames = 0;

            /** A non-zero cap paces the headless self-pump; zero is uncapped. */
            unsigned int maxFps = 0;
        };

        void run(std::function<bool(Frame const&)> frameCallback) override;
        void pauseFrames() override;
        void resumeFrames() override;
        bool stepFrame(std::chrono::microseconds dt) override;
        void requestFrame() override;
        btl::RunLoop& runLoop() override;

    protected:
        /** @brief Bind the platform to the run loop it drives frames on. The
         * loop is created and owned by the caller (App, a test) and injected
         * here; it must outlive the platform, which holds it only by reference.
         */
        explicit PlatformBase(btl::RunLoop& loop);

        /** @brief Drain the backend's OS event source, firing the window
         * handlers. Called by the loop when its `wakeSource()` signals input; a
         * headless backend with no OS source leaves this a no-op. */
        virtual void handleEvents() = 0;

        /** @brief The static cadence and headless self-pump parameters the
         * shared `run` loop uses. The default is an on-demand loop; each backend
         * overrides what it needs. */
        virtual RunConfig runConfig();

        /** @brief The OS handle the loop waits on for input, drained via
         * `handleEvents()`. Invalid by default -- a headless backend with no OS
         * event source. Called once, at loop start. */
        virtual btl::NativeHandle wakeSource();

        /** @brief The backend's live list of windows the loop renders each dirty
         * tick. The loop re-reads it every tick because windows open and close
         * during a run, so it is never snapshotted. */
        virtual std::vector<std::weak_ptr<WindowBase>>& getRenderWindows() = 0;

        // The injected run loop, owned by the caller and outliving this
        // platform. Held by reference: the platform no longer owns its loop.
        btl::RunLoop& loop_;

        // Set to true by requestFrame() (possibly off-thread) to coalesce a
        // burst of wake requests into a single posted task.
        std::atomic<bool> wakePosted_ = false;

        // Installed by a running run() so requestFrame(), which cannot see the
        // loop-local tick, can schedule one while the loop is active; cleared
        // when run() returns.
        std::function<void(btl::RunLoop::Controller&)> scheduleTick_;

        // Installed by a running run() so a pause token can produce one frame via
        // stepFrame(); cleared when run() returns.
        std::function<bool(std::chrono::microseconds)> stepFrame_;

        // Depth of outstanding pause tokens. Non-zero suspends auto-cadence frame
        // production; the loop keeps pumping events regardless.
        int pauseCount_ = 0;
    };
}
