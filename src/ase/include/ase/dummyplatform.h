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
         * @brief Advance exactly one frame at the supplied time: run the app
         * callback, then each live window's frame callback.
         *
         * Clock-agnostic — the caller supplies the frame — so run() pumps fixed
         * deterministic frames and an external driver can supply its own.
         *
         * @return Whether the app callback asked to keep running.
         */
        bool step(Frame const& frame,
                std::function<bool(Frame const&)> const& frameCallback) override;

        /**
         * @brief Pump frames until the app frame callback returns false or the
         * frame budget is reached.
         *
         * Each frame uses the fixed `dt` and advances a deterministic clock.
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
