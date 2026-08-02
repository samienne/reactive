#pragma once

#include "platformimpl.h"

#include "asevisibility.h"

#include <btl/runloop.h>

#include <atomic>
#include <cstdint>
#include <functional>

namespace ase
{
    class Platform;

    class ASE_EXPORT DummyPlatform : public PlatformImpl
    {
    public:
        Window makeWindow(Vector2i size) override;
        void handleEvents() override;
        RenderContext makeRenderContext() override;
        void run(RenderContext& renderContext,
                std::function<bool(Frame const&)> frameCallback) override;
        void requestFrame() override;

        /** @brief Cap the headless frame rate. Zero (the default) leaves the loop
         * uncapped: a tick runs as soon as requestFrame() wakes it. A positive
         * value paces ticks to at most fps frames per second. Set before run().
         */
        void setMaxFps(unsigned int fps);

        /**
         * @brief Cap the number of frames run() will pump.
         *
         * Zero (the default) means unbounded: run() is on-demand and blocks
         * between frames, so it never spins. A non-zero budget makes run() pump
         * deterministically up to that many frames then stop, which is how a
         * headless (non-agentic) app run terminates without an OS window.
         */
        void setMaxFrames(uint64_t maxFrames);

    private:
        // Set to true by requestFrame() (possibly off-thread) to coalesce a
        // burst of wake requests into a single posted task.
        std::atomic<bool> wakePosted_ = false;

        // Installed by run() so requestFrame(), which cannot see run()'s local
        // tick, can schedule one while the loop is active; cleared at run() exit.
        std::function<void(btl::RunLoop::Controller&)> scheduleTick_;

        // Frame-rate cap; 0 means uncapped. Read only on the loop thread, so it
        // must be set before run().
        unsigned int maxFps_ = 0;

        // Frame budget for run(); zero means unbounded (on-demand).
        uint64_t maxFrames_ = 0;
    };

    /**
     * @brief Construct a headless platform explicitly, independent of the
     * build's default backend.
     *
     * `makeDefaultPlatform()` selects the OS backend; this always gives the
     * dummy one.
     */
    ASE_EXPORT Platform makeDummyPlatform();

} // namespace ase
