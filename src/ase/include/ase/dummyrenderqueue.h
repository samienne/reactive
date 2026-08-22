#pragma once

#include "renderqueueimpl.h"

#include <btl/future/promise.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace ase
{
    class DummyRenderQueue : public RenderQueueImpl
    {
    public:
        /** @brief Headless render queue.
         *
         * With a zero interval fences complete inline on submit(). A positive
         * interval defers each fence by that much wall-clock time on a background
         * thread, mimicking a vsync'd GPU so acquire() backpressure paces the
         * loop as it would on a real window.
         */
        explicit DummyRenderQueue(
                std::chrono::microseconds frameInterval = std::chrono::microseconds{ 0 });
        ~DummyRenderQueue() override;

        // From RenderQueueImpl
        void flush() override;
        void finish() override;
        void submit(CommandBuffer&& renderQueue) override;

    private:
        struct PendingFence
        {
            btl::future::Promise<> promise;
            std::chrono::steady_clock::time_point targetTime;
        };

        void completerLoop();

        std::chrono::microseconds frameInterval_;

        std::mutex mutex_;
        std::condition_variable condition_;
        bool shutdown_ = false;
        std::vector<PendingFence> pending_;

        std::thread completer_;
    };
} // namespace ase
