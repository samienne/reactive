#pragma once

#include "vector.h"
#include "asevisibility.h"

#include <btl/nativehandle.h>
#include <btl/runloop.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <typeindex>
#include <typeinfo>
#include <vector>

namespace ase
{
    class RenderContext;
    class Window;
    class WindowImpl;
    struct Frame;

    class ASE_EXPORT PlatformImpl
    {
    public:
        /** @brief What a backend hands the shared frame loop to drive its frames:
         * its render list, its wake source, the cadence it targets, and (headless)
         * its self-pump budget. Everything that differs between GLX, WGL and the
         * dummy; the loop body itself is the same for all three. */
        struct RunConfig
        {
            /** The handle the loop blocks on for input; left invalid (the
             * default) by a headless backend with no OS event source. */
            btl::NativeHandle wakeSource;

            /** The interval the frame cadence targets. */
            std::chrono::microseconds frameStep = std::chrono::microseconds(16667);

            /** A non-zero budget bounds the run and makes it self-pump to that
             * many frames (headless); zero leaves the loop on-demand. */
            std::uint64_t maxFrames = 0;

            /** A non-zero cap paces the headless self-pump; zero is uncapped. */
            unsigned int maxFps = 0;

            /** The windows the loop renders each dirty tick, or null for a
             * backend with no drawable surfaces (the dummy). Owned by the
             * backend; it must outlive the run. */
            std::vector<std::weak_ptr<WindowImpl>>* renderWindows = nullptr;
        };

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

        /** @brief Take the injected run loop and drive frames on it until the
         * frame callback returns false (or, headless, the frame budget is spent).
         * The one shared loop body every backend runs: per dirty window it gates
         * on that window's own backpressure (`acquire`), renders and presents it
         * through the context the window carries, and fences it on the window's
         * own queue -- so the loop names no context of its own. `RunConfig`
         * supplies what differs between backends. */
        void run(std::function<bool(Frame const&)> frameCallback);

        /** @brief Suspend auto-cadence frame production while keeping the loop
         * pumping events and IO. Balanced by `resumeFrames`; the pause token owns
         * that pairing. */
        void pauseFrames();

        /** @brief End one `pauseFrames`; when the last is undone, wake the loop so
         * the cadence resumes. */
        void resumeFrames();

        /** @brief Produce exactly one frame on demand -- the same callback and
         * render path an auto tick takes, on a caller-supplied `dt` -- and report
         * whether the app wants to keep running. A no-op returning false when no
         * run is active. Called on the loop thread (from a loop source). */
        bool stepFrame(std::chrono::microseconds dt);

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
        /** @brief The backend-specific driving parameters the shared `run` loop
         * uses (render list, wake source, cadence, headless budget). The default
         * is an on-demand loop with no windows and no wake source; each backend
         * overrides what it needs. */
        virtual RunConfig runConfig()
        {
            return RunConfig{};
        }

        // The injected run loop, owned by the caller and outliving this
        // platform. Held by reference: the platform no longer owns its loop.
        btl::RunLoop& loop_;

        // Set to true by requestFrame() (possibly off-thread) to coalesce a
        // burst of wake requests into a single posted task.
        std::atomic<bool> wakePosted_ = false;

        // Installed by a running run() so requestFrame(), which cannot see the
        // loop-local tick, can schedule one while the loop is active; cleared
        // when run() returns.
        std::function<void(btl::RunLoop::Controller&)> scheduleTick_;

        // Installed by a running run() so a pause token can produce one frame via
        // stepFrame(); cleared when run() returns.
        std::function<bool(std::chrono::microseconds)> stepFrame_;

        // Depth of outstanding pause tokens. Non-zero suspends auto-cadence frame
        // production; the loop keeps pumping events regardless.
        int pauseCount_ = 0;
    };
}
