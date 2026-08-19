#pragma once

#include "platformbase.h"

#include "asevisibility.h"

#include <btl/runloop.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace ase
{
    class Platform;
    class WindowBase;

    class ASE_EXPORT DummyPlatform : public PlatformBase
    {
    public:
        explicit DummyPlatform(btl::RunLoop& loop);

        Window makeWindow(RenderContext& context, Vector2i size,
                bool headless) override;
        RenderContext makeRenderContext() override;

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

    protected:
        void handleEvents() override;
        RunConfig runConfig() override;
        std::vector<std::weak_ptr<WindowBase>>& getRenderWindows() override;

    private:
        // Frame-rate cap; 0 means uncapped. Read only on the loop thread, so it
        // must be set before run().
        unsigned int maxFps_ = 0;

        // Frame budget for run(); zero means unbounded (on-demand).
        uint64_t maxFrames_ = 0;

        std::vector<std::weak_ptr<WindowBase>> renderWindows_;
    };

    /**
     * @brief Construct a headless platform explicitly, independent of the
     * build's default backend.
     *
     * `makeDefaultPlatform()` selects the OS backend; this always gives the
     * dummy one. `loop` is injected and must outlive the returned platform.
     */
    ASE_EXPORT Platform makeDummyPlatform(btl::RunLoop& loop);

} // namespace ase

