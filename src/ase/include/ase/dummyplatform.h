#pragma once

#include "platformimpl.h"

#include "asevisibility.h"

#include <btl/runloop.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace ase
{
    class Platform;
    class WindowImpl;

    class ASE_EXPORT DummyPlatform : public PlatformImpl
    {
    public:
        Window makeWindow(Vector2i size) override;
        Window makeOffscreenWindow(RenderContext& context,
                Vector2i size) override;
        void handleEvents() override;
        RenderContext makeRenderContext() override;
        void run(RenderContext& renderContext,
                std::function<bool(Frame const&)> frameCallback) override;
        Session makeSession(RenderContext& context) override;

        /** @brief Cap the headless frame rate. Zero (the default) leaves the loop
         * uncapped: a tick runs as soon as requestFrame() wakes it. A positive
         * value paces ticks to at most fps frames per second. Set before run().
         */
        void setMaxFps(unsigned int fps);

        /**
         * @brief Cap the number of frames run() will pump.
         *
         * Zero (the default) is unbounded: run() never self-terminates. A
         * non-zero budget stops run() after that many frames, letting a headless
         * app run terminate without an OS window. Set before run().
         */
        void setMaxFrames(uint64_t maxFrames);

    private:
        // Frame-rate cap; 0 means uncapped. Read only on the loop thread, so it
        // must be set before run().
        unsigned int maxFps_ = 0;

        // Frame budget for run(); zero means unbounded (on-demand).
        uint64_t maxFrames_ = 0;

        // No surface is ever registered here -- the dummy is a Session with a
        // no-op render path -- but the Session drives its render list by
        // reference, so it holds an (always empty) one to hand over.
        std::vector<std::weak_ptr<WindowImpl>> renderWindows_;
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

