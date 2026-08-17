#pragma once

#include "vector.h"
#include "asevisibility.h"

#include <btl/runloop.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <typeindex>
#include <typeinfo>

namespace ase
{
    class RenderContext;
    class Window;
    class Session;

    class ASE_EXPORT PlatformImpl
    {
    public:
        /** @brief Bind the platform to the run loop it drives frames on. The
         * loop is created and owned by the caller (App, a test) and injected
         * here; it must outlive the platform, which holds it only by reference.
         */
        explicit PlatformImpl(btl::RunLoop& loop) :
            loop_(loop)
        {
        }

        virtual ~PlatformImpl() = default;

        /** @brief Reach the impl of a given concrete type through any decorators
         * wrapping this platform, or null if none in the chain has that type,
         * where `getImpl` would throw. Same-binary only. */
        virtual PlatformImpl* getImplOfType(std::type_index type)
        {
            return std::type_index(typeid(*this)) == type ? this : nullptr;
        }

        virtual Window makeWindow(RenderContext& context, Vector2i size,
                bool headless) = 0;
        virtual void handleEvents() = 0;
        virtual RenderContext makeRenderContext() = 0;

        /** @brief Build the Session that drives this backend's frames: its render
         * list, event pump and wake source bundled with `context`. The caller
         * owns the returned Session and runs it; a headless backend bounds it by
         * its frame budget. */
        virtual Session makeSession(RenderContext& context) = 0;

        /** @brief Wake the frame loop so it runs a tick and re-evaluates. Safe to
         * call off the loop thread. A decorator that drives frames itself
         * overrides this. */
        virtual void requestFrame()
        {
            if (!wakePosted_.exchange(true))
            {
                loop_.post([this](btl::RunLoop::Controller& controller)
                    {
                        wakePosted_ = false;
                        if (scheduleTick_)
                            scheduleTick_(controller);
                    });
            }
        }

        /** @brief The platform's run loop, for registering sources (sockets,
         * timers) serviced alongside the frame loop.
         */
        btl::RunLoop& runLoop()
        {
            return loop_;
        }

    protected:
        // The injected run loop, owned by the caller and outliving this
        // platform. Held by reference: the platform no longer owns its loop.
        btl::RunLoop& loop_;

        // Set to true by requestFrame() (possibly off-thread) to coalesce a
        // burst of wake requests into a single posted task.
        std::atomic<bool> wakePosted_ = false;

        // Installed by the running Session so requestFrame(), which cannot see
        // the loop-local tick, can schedule one while the loop is active; cleared
        // when the Session's run() returns.
        std::function<void(btl::RunLoop::Controller&)> scheduleTick_;

        friend class Session;
    };
}

