#pragma once

#include "nativehandle.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

namespace btl
{
    class RunLoopImpl;

    using SourceId = std::uint64_t;
    using TimerId = std::uint64_t;

    /** @brief A single-threaded reactor over readable/writable/timer sources
     * and cross-thread posted tasks.
     *
     * Register sources through a Controller, then call run() to hand it the
     * thread; run() returns when stop() is called. Only post() and stop() are
     * safe off the loop thread -- the rest lives on Controller.
     *
     * Readiness is level-triggered: a readable/writable callback fires while the
     * handle stays ready, so drain it each call. Destroy a RunLoop on its loop
     * thread, or after run() has returned.
     */
    class RunLoop
    {
    public:
        class Controller;
        class Source;
        class Timer;

        RunLoop();
        ~RunLoop();

        RunLoop(RunLoop&&) = delete;
        RunLoop& operator=(RunLoop&&) = delete;

        RunLoop(RunLoop const&) = delete;
        RunLoop& operator=(RunLoop const&) = delete;

        /** @brief Tag selecting the default-registering constructor.
         *
         * For building a default loop in place (optional::emplace, make_unique),
         * where makeDefault()'s by-value return will not do -- RunLoop is
         * non-movable.
         */
        struct DefaultTag {};

        /** @brief Construct a loop registered as the process default.
         *
         * Lets code that cannot be handed the loop directly reach it through
         * getDefault()/tryGetDefault(). Throws std::runtime_error if a default
         * already exists; the registration is released on destruction.
         */
        explicit RunLoop(DefaultTag);

        /** @brief Construct a default-registered loop by value.
         *
         * Equivalent to RunLoop(DefaultTag{}).
         */
        static RunLoop makeDefault();

        /** @brief The process default loop; throws std::runtime_error if none is
         * registered. */
        static RunLoop& getDefault();

        /** @brief The process default loop, or nullptr if none is registered. */
        static RunLoop* tryGetDefault() noexcept;

        /** @brief Take the thread and dispatch sources until stop(). */
        void run();

        /** @brief Run task on the loop thread. Safe to call from any thread. */
        void post(std::function<void(Controller&)> task);

        /** @brief Ask run() to return. Safe from any thread or callback. */
        void stop();

    private:
        bool isDefault_ = false;
        std::shared_ptr<RunLoopImpl> impl_;
    };

    /** @brief The loop-thread interface handed to every callback.
     *
     * Register, remove, post, and stop through it; valid only for the length of
     * the callback, so do not store it.
     */
    class RunLoop::Controller
    {
    public:
        Controller(Controller const&) = delete;
        Controller& operator=(Controller const&) = delete;
        Controller(Controller&&) = delete;
        Controller& operator=(Controller&&) = delete;

        /** @brief Call onReadable while handle is readable. */
        [[nodiscard]] Source addReadable(NativeHandle handle,
                std::function<void(Controller&)> onReadable);

        /** @brief Call onWritable while handle is writable. */
        [[nodiscard]] Source addWritable(NativeHandle handle,
                std::function<void(Controller&)> onWritable);

        /** @brief Call callback once, after at least delay has elapsed. */
        [[nodiscard]] Timer addTimer(std::chrono::microseconds delay,
                std::function<void(Controller&)> callback);

        /** @brief Run task on the loop thread. Safe to call from any thread. */
        void post(std::function<void(Controller&)> task);

        /** @brief Ask run() to return. */
        void stop();

        void remove(SourceId id);
        void cancel(TimerId id);

    private:
        friend class RunLoopImpl;
        explicit Controller(RunLoopImpl* impl) : impl_(impl) {}

        RunLoopImpl* impl_;
    };

    /** @brief Owns a readable/writable registration, removing it when dropped.
     *
     * Call detach() to keep the source registered past the handle's life.
     */
    class RunLoop::Source
    {
    public:
        Source() = default;
        ~Source();

        Source(Source&& other) noexcept;
        Source& operator=(Source&& other) noexcept;

        Source(Source const&) = delete;
        Source& operator=(Source const&) = delete;

        SourceId id() const noexcept { return id_; }

        void detach() noexcept
        {
            weak_.reset();
            id_ = 0;
        }

    private:
        friend class RunLoop::Controller;
        Source(std::weak_ptr<RunLoopImpl> impl, SourceId id) :
            weak_(std::move(impl)), id_(id)
        {
        }

        void reset() noexcept;

        std::weak_ptr<RunLoopImpl> weak_;
        SourceId id_ = 0;
    };

    /** @brief Owns a timer registration, cancelling it when dropped.
     *
     * Call detach() to let a one-shot timer fire without holding the handle.
     */
    class RunLoop::Timer
    {
    public:
        Timer() = default;
        ~Timer();

        Timer(Timer&& other) noexcept;
        Timer& operator=(Timer&& other) noexcept;

        Timer(Timer const&) = delete;
        Timer& operator=(Timer const&) = delete;

        TimerId id() const noexcept { return id_; }

        void detach() noexcept
        {
            weak_.reset();
            id_ = 0;
        }

    private:
        friend class RunLoop::Controller;
        Timer(std::weak_ptr<RunLoopImpl> impl, TimerId id) :
            weak_(std::move(impl)), id_(id)
        {
        }

        void reset() noexcept;

        std::weak_ptr<RunLoopImpl> weak_;
        TimerId id_ = 0;
    };
}
