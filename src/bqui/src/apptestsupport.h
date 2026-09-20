#pragma once

#include "bqui/app.h"
#include "bqui/bquivisibility.h"

#include <chrono>
#include <cstddef>
#include <string>

namespace btl
{
    class RunLoop;
}

namespace bqui::test
{
    /**
     * @brief Test-only access to a mounted window's real input dispatch.
     *
     * Lets a test drive the genuine hit-test -> onClick path from the app
     * thread. Only valid to call from within App::run() (e.g. a `running`
     * signal), where the window impls exist. Not part of the shipped public
     * interface -- this header lives under src/bqui/src, not the exported
     * include/ tree.
     */
    struct BQUI_EXPORT WindowInput
    {
        /**
         * @brief Number of mounted windows.
         */
        static std::size_t count(App const& app);

        /**
         * @brief Center of the first snapshot text run equal to @p caption.
         *
         * In window @p index, in window pixel space. Returns false if there is
         * no such text run.
         */
        static bool findText(App const& app, std::size_t index,
                std::string const& caption, float& outX, float& outY);

        /**
         * @brief Injects a real pointer button down then up (a click).
         *
         * At (@p x, @p y) into window @p index, through the window's genuine
         * input path.
         */
        static void injectClick(App& app, std::size_t index, float x, float y);
    };

    /**
     * @brief Test-only deterministic headless frame driver.
     *
     * Runs an app on a caller-owned loop and produces a fixed number of frames
     * through the platform's manual pause/step path, then stops -- the bounded,
     * cadence-independent alternative to letting the run loop pace frames. The
     * app must be configured with a platform bound to the caller's loop first.
     */
    struct BQUI_EXPORT FrameDriver
    {
        /**
         * @brief Run @p app, producing exactly @p frames explicit frames,
         * advancing @p dt each, then stop.
         *
         * @return App::run's result.
         */
        static int run(App& app, btl::RunLoop& loop, std::size_t frames,
                std::chrono::microseconds dt);
    };
}
