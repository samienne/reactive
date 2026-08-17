#pragma once

#include "vector.h"
#include "asevisibility.h"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <typeindex>
#include <typeinfo>

namespace btl
{
    class RunLoop;
}

namespace ase
{
    class Window;
    class RenderContext;
    class PlatformImpl;
    struct Frame;

    /** @brief A RAII handle that suspends the platform's auto-cadence frames for
     * as long as it is held, and drives frames one at a time in the meantime.
     *
     * `Platform::pause()` returns one; while it lives the loop keeps pumping
     * events and IO but produces no auto frames, and `step(dt)` produces exactly
     * one. Dropping it resumes the free-running cadence. Move-only: the token's
     * lifetime is the paused mode, so it cannot be copied. Because `step` lives
     * only here, a free-running loop cannot be stepped by construction.
     *
     * Loop-thread-only: `Platform::pause()`, `step(dt)`, resuming (this token's
     * destruction), and this token's own destruction all mutate or read the
     * platform's unsynchronized pause state, so they must be called from the
     * loop thread. Safe because the loop is single-threaded. */
    class ASE_EXPORT PauseToken
    {
    public:
        PauseToken(PauseToken&& other) noexcept;
        PauseToken& operator=(PauseToken&& other) noexcept;
        PauseToken(PauseToken const&) = delete;
        PauseToken& operator=(PauseToken const&) = delete;
        ~PauseToken();

        /** @brief Produce exactly one frame -- the same frame-callback and render
         * path an auto tick takes, on the caller-supplied `dt` -- and report
         * whether the app wants to keep running. Call from the loop thread. */
        bool step(std::chrono::microseconds dt);

    private:
        friend class Platform;
        explicit PauseToken(std::shared_ptr<PlatformImpl> platform);

        std::shared_ptr<PlatformImpl> platform_;
    };

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

        /** @brief The run loop this platform was injected with and drives frames
         * on. */
        btl::RunLoop& runLoop();

        /** @brief Drive frames on the injected run loop until `frameCallback`
         * returns false. Context-free: each dirty window renders and presents
         * through the context it carries, so the loop names none of its own. */
        void run(std::function<bool(Frame const&)> frameCallback);

        /** @brief Suspend the auto-cadence frames and take manual control: the
         * returned token's `step(dt)` produces frames one at a time until it is
         * dropped, which resumes the free-running cadence. The loop keeps pumping
         * events and IO throughout. Loop-thread-only, like the returned token's
         * `step` and destruction: they touch the unsynchronized pause state and
         * are safe only because the loop is single-threaded. */
        PauseToken pause();

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

    /** @brief The OS default backend, bound to `loop`. The loop is created and
     * owned by the caller and injected here; it must outlive the platform. */
    ASE_EXPORT Platform makeDefaultPlatform(btl::RunLoop& loop);
} // ase

