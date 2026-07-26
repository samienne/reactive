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
     * Register the sources you need, then call run() to hand it the thread. The
     * loop dispatches each source's callback as it fires and returns when
     * stop() is called. It knows nothing about graphics, windows, or vsync -
     * those are sources a higher layer registers.
     *
     * Contract:
     *  - Readiness is level-triggered: a readable/writable callback fires while
     *    the handle stays ready, so drain it (or remove the source) each call.
     *  - One thread owns a RunLoop and its sources. post() is the only method
     *    safe to call from another thread; it wakes the loop and runs the task
     *    on the loop thread.
     *  - A source may remove() or cancel() itself from inside its callback.
     */
    class RunLoop
    {
    public:
        RunLoop();
        ~RunLoop();

        RunLoop(RunLoop&&) noexcept;
        RunLoop& operator=(RunLoop&&) noexcept;

        RunLoop(RunLoop const&) = delete;
        RunLoop& operator=(RunLoop const&) = delete;

        /** @brief Call onReadable while handle is readable. */
        SourceId addReadable(NativeHandle handle, std::function<void()> onReadable);

        /** @brief Call onWritable while handle is writable. */
        SourceId addWritable(NativeHandle handle, std::function<void()> onWritable);

        /** @brief Call callback once, after at least delay has elapsed. */
        TimerId addTimer(std::chrono::microseconds delay,
                std::function<void()> callback);

        /** @brief Run task on the loop thread. Safe to call from any thread. */
        void post(std::function<void()> task);

        void remove(SourceId id);
        void cancel(TimerId id);

        /** @brief Take the thread and dispatch sources until stop(). */
        void run();

        /** @brief Ask run() to return. Safe from any thread or callback. */
        void stop();

    private:
        std::unique_ptr<RunLoopImpl> impl_;
    };
}
