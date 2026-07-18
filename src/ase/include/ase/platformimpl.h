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

        // Advance exactly one frame at the supplied time: handle pending events,
        // invoke the app frame callback, then drive each window's frame. It does
        // not sample any clock — the caller supplies the frame — so an external
        // driver can step the platform with a controlled time. Returns whether
        // the app callback asked to keep running.
        virtual bool step(Frame const& frame,
                std::function<bool(Frame const&)> const& frameCallback) = 0;

        virtual void run(RenderContext& renderContext,
                std::function<bool(Frame const&)> frameCallback) = 0;
        virtual void requestFrame() = 0;
    };
}

