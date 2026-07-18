#pragma once

#include "platformimpl.h"

#include "asevisibility.h"

#include <cstdint>
#include <chrono>
#include <memory>
#include <vector>

namespace ase
{
    class DummyWindow;
    class Platform;

    /**
     * @brief A headless platform backend.
     *
     * It creates no OS window and drives a deterministic frame loop with a fixed
     * time step, so runs are reproducible and can be pumped a bounded number of
     * frames rather than free-running.
     */
    class ASE_EXPORT DummyPlatform : public PlatformImpl
    {
    public:
        DummyPlatform();

        Window makeWindow(Vector2i size) override;
        void handleEvents() override;
        RenderContext makeRenderContext() override;

        /**
         * @brief Pump frames until the app frame callback returns false or the
         * frame budget is reached.
         *
         * Each frame uses the fixed `dt` and advances a deterministic clock; the
         * app callback runs first, then each live window's frame callback.
         */
        void run(RenderContext& renderContext,
                std::function<bool(Frame const&)> frameCallback) override;
        void requestFrame() override;

        /**
         * @brief Fixed per-frame delta used for every stepped frame (default
         * 16.667ms ~ 60fps). Set before run() for a reproducible dt.
         */
        void setFrameDelta(std::chrono::microseconds dt);

        /**
         * @brief Cap the number of frames run() will pump.
         *
         * Zero means unbounded (only the app callback returning false stops it).
         * Default is bounded so a headless run cannot spin forever.
         */
        void setMaxFrames(uint64_t maxFrames);

        /**
         * @brief Advance exactly one frame: run the app callback and each window
         * frame with the fixed dt.
         *
         * @return Whether the app callback asked to keep running. Lets a caller
         * pump frames on demand.
         */
        bool step(std::function<bool(Frame const&)> const& frameCallback);

    private:
        std::vector<std::weak_ptr<DummyWindow>> windows_;
        std::chrono::microseconds time_ = std::chrono::microseconds(0);
        std::chrono::microseconds dt_ = std::chrono::microseconds(16667);
        uint64_t maxFrames_ = 600;
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
