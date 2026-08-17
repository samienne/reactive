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

    /** @brief A single-threaded reactor: a loop over readable/writable/timer
     * sources plus cross-thread posted tasks.
     *
     * Register the sources you need through a Controller, then call run() to hand
     * it the thread. The loop dispatches each source's callback as it fires and
     * returns when stop() is called. It knows nothing about graphics, windows, or
     * vsync - those are sources a higher layer registers.
     *
     * Only post() and stop() are safe to call from another thread. The rest of
     * the interface lives on Controller, which the loop hands to every callback,
     * so it is reachable only on the loop thread.
     *
     * Contract:
     *  - Readiness is level-triggered: a readable/writable callback fires while
     *    the handle stays ready, so drain it (or drop the source) each call.
     *  - Destroy a RunLoop on its loop thread, or after run() has returned.
     */
    class RunLoop
    {
    public:
        class Controller;
        class Source;
        class Timer;

        RunLoop();
        ~RunLoop();

        // Non-movable and non-copyable: the process default is a bare pointer to
        // a RunLoop, and code holds it by reference, so a live loop must keep its
        // address. Guaranteed copy elision keeps `auto loop = makeDefault();`
        // working despite this.
        RunLoop(RunLoop&&) = delete;
        RunLoop& operator=(RunLoop&&) = delete;

        RunLoop(RunLoop const&) = delete;
        RunLoop& operator=(RunLoop const&) = delete;

        /** @brief Tag selecting the default-registering constructor. Use it to
         * build a default loop in place -- optional::emplace, make_unique --
         * where makeDefault()'s by-value return will not do because a RunLoop is
         * non-movable. */
        struct DefaultTag {};

        /** @brief Construct a loop registered as the process default, so code
         * that cannot be handed the loop directly (socket IO, for one) can reach
         * it through getDefault()/tryGetDefault(). Throws std::runtime_error if a
         * default already exists. The registration is RAII: it is released when
         * the loop is destroyed, so the default can never dangle. A plain RunLoop
         * leaves the default untouched. */
        explicit RunLoop(DefaultTag);

        /** @brief Construct a default-registered loop by value; equivalent to
         * RunLoop(DefaultTag{}). Guaranteed copy elision keeps this valid despite
         * RunLoop being non-movable. */
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
        // True only for a loop built through makeDefault(): it registers itself
        // as the process default on construction and clears that registration on
        // destruction (compare-exchange, so only if it is still the default).
        bool isDefault_ = false;
        std::shared_ptr<RunLoopImpl> impl_;
    };

    /** @brief The loop-thread interface, handed to every callback for the length
     * of that call. Register, remove, post, and stop through it; do not store it.
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

    /** @brief Owns a readable/writable registration and removes it when dropped.
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

    /** @brief Owns a timer registration and cancels it when dropped. Call
     * detach() to let a one-shot timer fire without holding the handle.
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
