#pragma once

#include "vector.h"
#include "asevisibility.h"

#include <chrono>
#include <functional>
#include <optional>

namespace ase
{
    class RenderContext;
    class Window;
    struct Frame;

    class ASE_EXPORT PlatformImpl
    {
    public:
        virtual ~PlatformImpl() = default;

        virtual Window makeWindow(Vector2i size) = 0;
        virtual void handleEvents() = 0;
        virtual RenderContext makeRenderContext() = 0;

        /**
         * @brief Inject one frame into the windows: advance each live window at
         * the supplied time.
         *
         * Samples no clock and touches neither OS events nor the app callback
         * (those belong to run()'s loop), so an external driver can drive it with
         * a controlled time.
         */
        virtual void step(Frame const& frame) = 0;

        virtual void run(RenderContext& renderContext,
                std::function<bool(Frame const&)> frameCallback) = 0;
        virtual void requestFrame() = 0;
    };
}

