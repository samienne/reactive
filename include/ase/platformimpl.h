#pragma once

#include "vector.h"
#include "asevisibility.h"

#include <chrono>

namespace ase
{
    class RenderContext;
    class Window;
    struct Frame;

    class ASE_EXPORT PlatformImpl
    {
    public:
        virtual ~PlatformImpl() = default;

        virtual std::optional<std::chrono::microseconds> frame(Frame const& frame) = 0;
        virtual Window makeWindow(Vector2i size) = 0;
        virtual void handleEvents() = 0;
        virtual RenderContext makeRenderContext() = 0;
        virtual void run(RenderContext& renderContext,
                std::function<bool(Frame const&)> frameCallback) = 0;
        virtual void requestFrame() = 0;
    };
}

