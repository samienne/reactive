#include "basicfence.h"

#include <btl/async.h>

#include <iterator>

namespace ase
{

bool BasicFence::isSignaled() const
{
    return signaled_.load(std::memory_order_acquire);
}

btl::future::Future<> BasicFence::whenSignaled()
{
    auto control = std::make_shared<btl::future::FutureControl<>>();
    btl::future::Promise<> promise(control);

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!signaled_.load(std::memory_order_relaxed))
        {
            promises_.push_back(std::move(promise));
            return { std::move(control) };
        }
    }

    promise.set();

    return { std::move(control) };
}

void BasicFence::signal(std::vector<btl::future::Promise<>>& signaled)
{
    std::lock_guard<std::mutex> lock(mutex_);

    signaled_.store(true, std::memory_order_release);

    signaled.insert(signaled.end(),
            std::make_move_iterator(promises_.begin()),
            std::make_move_iterator(promises_.end()));
    promises_.clear();
}

void FenceDrain::post(std::vector<btl::future::Promise<>> promises)
{
    if (promises.empty())
        return;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        pending_.insert(pending_.end(),
                std::make_move_iterator(promises.begin()),
                std::make_move_iterator(promises.end()));

        if (scheduled_)
            return;

        scheduled_ = true;
    }

    btl::asyncJob([self = shared_from_this()]
        {
            self->drain();
        });
}

void FenceDrain::drain()
{
    for (;;)
    {
        std::vector<btl::future::Promise<>> batch;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (pending_.empty())
            {
                scheduled_ = false;
                return;
            }

            batch.swap(pending_);
        }

        for (auto& promise : batch)
            promise.set();
    }
}

} // namespace ase
