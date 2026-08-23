#pragma once

#include "platformimpl.h"

#include <btl/nativehandle.h>
#include <btl/runloop.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace ase
{
    class WindowBase;
    struct Frame;

    /**
     * @brief The shared base every backend platform derives from.
     *
     * Owns the single frame-loop implementation -- `run()`, the pause/step
     * control, and `requestFrame()` over the injected run loop -- and leaves
     * each backend only its per-tick hooks: event draining, cadence config,
     * wake source, and the live render-window list.
     */
    class ASE_EXPORT PlatformBase : public PlatformImpl
    {
    public:
        /** @brief The static cadence the shared frame loop targets, plus the
         * headless self-pump parameters.
         *
         * Just the scalars that differ between GLX, WGL and the dummy backends.
         */
        struct RunConfig
        {
            /** The interval the frame cadence targets. */
            std::chrono::microseconds frameStep = std::chrono::microseconds(16667);

            /** A non-zero budget bounds the run and makes it self-pump to that
             * many frames (headless); zero leaves the loop on-demand. */
            std::uint64_t maxFrames = 0;
        };

        void run(std::function<bool(Frame const&)> frameCallback) override;
        void pauseFrames() override;
        void resumeFrames() override;
        bool stepFrame(std::chrono::microseconds dt) override;
        void requestFrame() override;
        btl::RunLoop& runLoop() override;

    protected:
        /** @brief Bind the platform to the run loop it drives frames on.
         *
         * The loop is owned by the caller (App, a test) and must outlive the
         * platform, which holds it only by reference.
         */
        explicit PlatformBase(btl::RunLoop& loop);

        /** @brief Drain the backend's OS event source, firing the window
         * handlers.
         *
         * Called by the loop when its `wakeSource()` signals input; a headless
         * backend with no OS source leaves this a no-op.
         */
        virtual void handleEvents() = 0;

        /** @brief The static cadence and headless self-pump parameters the
         * shared `run` loop uses.
         *
         * The default is an on-demand loop; each backend overrides what it
         * needs.
         */
        virtual RunConfig runConfig();

        /** @brief The OS handle the loop waits on for input, drained via
         * `handleEvents()`.
         *
         * Invalid by default -- a headless backend with no OS event source.
         * Called once, at loop start.
         */
        virtual btl::NativeHandle wakeSource();

        /** @brief The backend's live list of windows the loop renders each dirty
         * tick.
         *
         * Re-read every tick and never snapshotted, since windows open and close
         * during a run.
         */
        virtual std::vector<std::weak_ptr<WindowBase>>& getRenderWindows() = 0;

        btl::RunLoop& loop_;

        // Set (possibly off-thread) by requestFrame() to coalesce a burst of
        // wake requests into a single posted task.
        std::atomic<bool> wakePosted_ = false;

        // Installed while run() is active, cleared when it returns, so
        // requestFrame() and a pause token can reach the loop-local tick.
        std::function<void(btl::RunLoop::Controller&)> scheduleTick_;
        std::function<bool(std::chrono::microseconds)> stepFrame_;

        // Depth of outstanding pause tokens; non-zero suspends auto-cadence
        // frames.
        int pauseCount_ = 0;

    private:
        void renderDirtyWindows(Frame const& frame);

        // The earliest frame time a renderable window wants its next frame at,
        // or nullopt if none does; the loop schedules its next tick to this.
        std::optional<std::chrono::microseconds> earliestFrameTime();

        // Any window whose in-flight budget is full.
        bool anyWindowSaturated();
    };
}
