#pragma once

#include "asevisibility.h"

#include <btl/nativehandle.h>
#include <btl/runloop.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace ase
{
    class PlatformImpl;
    class RenderContext;
    class WindowImpl;
    struct Frame;

    /** @brief Owns the frame-loop body every backend drives frames through: the
     * render list, the frame clock and cadence, GPU backpressure, and the
     * on-demand re-arm. A platform supplies only what differs between backends --
     * its event pump and the handle the loop wakes on -- so GLX, WGL and the
     * headless dummy all run this one shared tick.
     */
    class ASE_EXPORT Session
    {
    public:
        /** @brief What a platform hands the Session to drive its frames. */
        struct Config
        {
            /** The platform's event pump, run before each wake-driven tick. */
            std::function<void()> handleEvents;

            /** The handle the loop blocks on for input; left invalid (the
             * default) by a headless backend with no OS event source. */
            btl::NativeHandle wakeSource;

            /** The interval the frame cadence targets. */
            std::chrono::microseconds frameStep;

            /** A non-zero budget bounds the run and makes it self-pump to that
             * many frames (headless); zero leaves the loop on-demand. */
            std::uint64_t maxFrames = 0;

            /** A non-zero cap paces the headless self-pump; zero is uncapped. */
            unsigned int maxFps = 0;
        };

        Session(PlatformImpl& platform, RenderContext& context,
                std::vector<std::weak_ptr<WindowImpl>>& renderWindows,
                Config config);

        Session(Session const&) = delete;
        Session& operator=(Session const&) = delete;

        /** @brief Take the platform's run loop and drive frames until the frame
         * callback returns false (or, headless, the frame budget is spent). */
        void run(std::function<bool(Frame const&)> frameCallback);

    private:
        PlatformImpl& platform_;
        RenderContext& context_;
        std::vector<std::weak_ptr<WindowImpl>>& renderWindows_;
        Config config_;
    };
} // namespace ase
