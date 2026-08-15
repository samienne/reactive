#pragma once

#include "vector.h"
#include "asevisibility.h"

#include <btl/runloop.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <typeindex>
#include <typeinfo>

namespace ase
{
    class RenderContext;
    class Window;
    class WindowImpl;
    struct Frame;

    class ASE_EXPORT PlatformImpl
    {
    public:
        virtual ~PlatformImpl() = default;

        /** @brief Reach the impl of a given concrete type through any decorators
         * wrapping this platform, or null if none in the chain has that type,
         * where `getImpl` would throw. Same-binary only. */
        virtual PlatformImpl* getImplOfType(std::type_index type)
        {
            return std::type_index(typeid(*this)) == type ? this : nullptr;
        }

        virtual Window makeWindow(Vector2i size) = 0;

        /** @brief Track an externally-made window in the render list, if this
         * platform keeps one. The default ignores it (a platform whose loop is
         * not window-driven, like the dummy backend). */
        virtual void registerRenderWindow(std::weak_ptr<WindowImpl>)
        {
        }

        virtual void handleEvents() = 0;
        virtual RenderContext makeRenderContext() = 0;
        virtual void run(RenderContext& renderContext,
                std::function<bool(Frame const&)> frameCallback) = 0;
        virtual void requestFrame() = 0;

        /** @brief The platform's run loop, for registering sources (sockets,
         * timers) serviced alongside the frame loop.
         */
        btl::RunLoop& runLoop()
        {
            return loop_;
        }

    protected:
        btl::RunLoop loop_;
    };
}

