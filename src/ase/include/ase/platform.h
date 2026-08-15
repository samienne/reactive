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
    struct Frame;

    class ASE_EXPORT Platform
    {
    public:
        Platform(std::shared_ptr<PlatformImpl> impl);
        virtual ~Platform();

        Window makeWindow(Vector2i size);

        /** @brief Track a window this platform did not itself make, so its run
         * loop draws it alongside the windows it did. For a window built by a
         * dependent that has the render context an FBO needs — an
         * `OffscreenWindow` — where `makeWindow` builds windows needing OS
         * handles the platform owns. Ownership stays with the caller; the
         * platform holds a weak reference and drops it once the window is gone. */
        void registerRenderWindow(Window const& window);

        void handleEvents();
        RenderContext makeRenderContext();
        void run(RenderContext& renderContext,
                std::function<bool(Frame const&)> frameCallback);
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

