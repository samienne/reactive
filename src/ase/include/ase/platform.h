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

    class ASE_EXPORT Platform
    {
    public:
        Platform(std::shared_ptr<PlatformImpl> impl);
        virtual ~Platform();

        /** @brief Make a window of this backend bound to `context`. Headless, it
         * renders offscreen into an FBO built from `context` and is never shown
         * (no present, `setVisible` is a no-op); otherwise it is the real OS
         * window. Either is driven from the run loop like any other window. */
        Window makeWindow(RenderContext& context, Vector2i size, bool headless);

        void handleEvents();
        RenderContext makeRenderContext();

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

