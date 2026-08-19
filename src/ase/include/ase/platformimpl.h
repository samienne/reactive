#pragma once

#include "vector.h"
#include "asevisibility.h"

#include <chrono>
#include <functional>
#include <memory>
#include <typeindex>
#include <typeinfo>

namespace btl
{
    class RunLoop;
}

namespace ase
{
    class RenderContext;
    class Window;
    struct Frame;

    /** @brief The platform's pure interface: the operations the `Platform`
     * handle and its `PauseToken` call, plus the `getImplOfType` type-erasure
     * plumbing. The shared frame-loop implementation lives one level down on
     * `PlatformBase`; every concrete backend derives from that. */
    class ASE_EXPORT PlatformImpl :
        public std::enable_shared_from_this<PlatformImpl>
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

        /** @overload Typed form: the impl as `T*`, or null if none in the chain
         * is a `T`. Same-binary only. */
        template <class T>
        T* getImplOfType()
        {
            return static_cast<T*>(getImplOfType(std::type_index(typeid(T))));
        }

        virtual Window makeWindow(RenderContext& context, Vector2i size,
                bool headless) = 0;
        virtual RenderContext makeRenderContext() = 0;

        /** @brief Drive frames on the injected run loop until the app quits.
         *
         * Runs until the frame callback returns false (or, headless, the frame
         * budget is spent). The one shared loop body every backend runs: per
         * dirty window it gates on that window's own backpressure (`acquire`),
         * renders and presents it through the context the window carries, and
         * fences it on the window's own queue -- so the loop names no context of
         * its own. */
        virtual void run(std::function<bool(Frame const&)> frameCallback) = 0;

        /** @brief Suspend auto-cadence frame production while keeping the loop
         * pumping events and IO. Balanced by `resumeFrames`; the pause token owns
         * that pairing. */
        virtual void pauseFrames() = 0;

        /** @brief End one `pauseFrames`; when the last is undone, wake the loop so
         * the cadence resumes. */
        virtual void resumeFrames() = 0;

        /** @brief Produce exactly one frame on demand -- the same callback and
         * render path an auto tick takes, on a caller-supplied `dt` -- and report
         * whether the app wants to keep running. A no-op returning false when no
         * run is active. Called on the loop thread (from a loop source). */
        virtual bool stepFrame(std::chrono::microseconds dt) = 0;

        /** @brief Wake the frame loop so it runs a tick and re-evaluates. Safe to
         * call off the loop thread. A decorator that drives frames itself
         * overrides this. */
        virtual void requestFrame() = 0;

        /** @brief The platform's run loop, for registering sources (sockets,
         * timers) serviced alongside the frame loop.
         */
        virtual btl::RunLoop& runLoop() = 0;
    };
}
