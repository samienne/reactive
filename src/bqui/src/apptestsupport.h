#pragma once

#include "bqui/app.h"
#include "bqui/bquivisibility.h"

#include <cstddef>
#include <string>

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
        /** @brief Number of mounted windows. */
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
}
