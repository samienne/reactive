#include "dummyrenderqueue.h"

#include "commandbuffer.h"
#include "rendercommand.h"

#include <algorithm>
#include <variant>

namespace ase
{

DummyRenderQueue::DummyRenderQueue(std::chrono::microseconds frameInterval) :
    frameInterval_(frameInterval)
{
    if (frameInterval_ > std::chrono::microseconds{ 0 })
        completer_ = std::thread([this] { completerLoop(); });
}

DummyRenderQueue::~DummyRenderQueue()
{
    if (!completer_.joinable())
        return;

    std::vector<PendingFence> due;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        due = std::move(pending_);
        pending_.clear();
    }
    condition_.notify_all();

    // Never leave a fence pending: an unfired one wedges window backpressure and
    // leaks the frame it holds.
    for (auto& p : due)
        p.promise.set();

    completer_.join();
}

void DummyRenderQueue::flush()
{
}

void DummyRenderQueue::finish()
{
    // Teardown drains in-flight frames through here, so complete every pending
    // fence now rather than waiting out its interval.
    std::vector<PendingFence> due;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        due = std::move(pending_);
        pending_.clear();
    }

    for (auto& p : due)
        p.promise.set();
}

void DummyRenderQueue::submit(CommandBuffer&& commandBuffer)
{
    // Nothing draws, but fences must still complete: the frame loop tracks GPU
    // frames in flight by a fence per submit, so leaving them pending would wedge
    // its backpressure.
    if (frameInterval_ == std::chrono::microseconds{ 0 })
    {
        for (auto& command : commandBuffer)
            if (std::holds_alternative<FenceCommand>(command))
                std::get<FenceCommand>(command).promise.set();
        return;
    }

    auto targetTime = std::chrono::steady_clock::now() + frameInterval_;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& command : commandBuffer)
            if (std::holds_alternative<FenceCommand>(command))
                pending_.push_back({
                        std::move(std::get<FenceCommand>(command).promise),
                        targetTime });
    }

    condition_.notify_one();
}

void DummyRenderQueue::completerLoop()
{
    std::unique_lock<std::mutex> lock(mutex_);

    while (!shutdown_)
    {
        if (pending_.empty())
        {
            condition_.wait(lock,
                    [this] { return shutdown_ || !pending_.empty(); });
            continue;
        }

        auto earliest = std::min_element(pending_.begin(), pending_.end(),
                [](PendingFence const& a, PendingFence const& b)
                { return a.targetTime < b.targetTime; });

        // Copy the deadline out: wait_until releases the lock, and a concurrent
        // submit() can then realloc pending_, dangling both the iterator and any
        // reference into its storage.
        auto targetTime = earliest->targetTime;

        if (std::chrono::steady_clock::now() < targetTime)
        {
            // Wake early if newer/nearer work arrives or on shutdown.
            condition_.wait_until(lock, targetTime);
            continue;
        }

        auto promise = std::move(earliest->promise);
        pending_.erase(earliest);

        lock.unlock();
        promise.set();
        lock.lock();
    }
}

} // namespace
