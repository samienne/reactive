#pragma once

#include "vector.h"
#include "asevisibility.h"

#include <chrono>
#include <memory>
#include <optional>
#include <typeindex>
#include <typeinfo>

namespace ase
{
    class Window;
    class RenderContext;
    class PlatformImpl;
    class Session;
    struct Frame;

    class ASE_EXPORT Platform
    {
    public:
        Platform(std::shared_ptr<PlatformImpl> impl);
        virtual ~Platform();

        Window makeWindow(Vector2i size);

        /** @brief Make a window that renders offscreen with this backend and is
         * never shown, drawn into an FBO built from `context`. The platform
         * drives it from its run loop like a normal window; there is no present
         * and `setVisible` is a no-op. */
        Window makeOffscreenWindow(RenderContext& context, Vector2i size);

        void handleEvents();
        RenderContext makeRenderContext();
        void run(RenderContext& renderContext,
                std::function<bool(Frame const&)> frameCallback);

        /** @brief Build the Session driving this platform's frames, bound to
         * `context`; reaches through any decorator to the backend that owns a
         * frame loop. */
        Session makeSession(RenderContext& context);
        void requestFrame();

        /** @brief The platform's implementation as concrete type `T`, reached
         * through any decorators wrapping it; throws `std::bad_cast` if no impl
         * in the chain has that type. Same-binary only. */
        template <typename T>
        T& getImpl()
        {
            PlatformImpl* impl = getImplOfType(std::type_index(typeid(T)));
            if (!impl)
                throw std::bad_cast();

            return static_cast<T&>(*impl);
        }

        template <typename T>
        T const& getImpl() const
        {
            return const_cast<Platform*>(this)->getImpl<T>();
        }

        /** @brief Reach the impl of a given concrete type through any decorators
         * wrapping this platform, or null if none in the chain has that type,
         * where `getImpl` would throw. Same-binary only. */
        PlatformImpl* getImplOfType(std::type_index type);

    private:
        PlatformImpl* d()
        {
            return deferred_.get();
        }

        PlatformImpl const* d() const
        {
            return deferred_.get();
        }

    private:
        std::shared_ptr<PlatformImpl> deferred_;
    };

    ASE_EXPORT Platform makeDefaultPlatform();
} // ase

