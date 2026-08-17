#include "windowimpl.h"

#include "rendercontext.h"
#include "renderqueue.h"
#include "commandbuffer.h"

#include <condition_variable>
#include <mutex>

namespace ase
{

// The window's backpressure state, shared with the fence completion callback so
// a fence can free a slot even after the window itself is gone.
struct WindowPresentSync
{
    std::mutex mutex;
    std::condition_variable slotFreed;

    // Frames whose draws are submitted but whose fence has not yet completed.
    int inFlight = 0;

    // How many of this window's frames may be in flight at once. Two keeps the
    // GPU fed (one presenting while the next builds) without unbounded latency.
    int budget = 2;
};

WindowImpl::WindowImpl(RenderContext& context) :
    context_(context.getSharedImpl()),
    presentSync_(std::make_shared<WindowPresentSync>())
{
}

RenderContext& WindowImpl::getRenderContext()
{
    return context_;
}

RenderQueue WindowImpl::getMainRenderQueue()
{
    return context_.getMainRenderQueue();
}

PresentStatus WindowImpl::acquire()
{
    auto sync = presentSync_;

    std::unique_lock<std::mutex> lock(sync->mutex);
    sync->slotFreed.wait(lock, [&sync] { return sync->inFlight < sync->budget; });

    return PresentStatus::Ok;
}

void WindowImpl::submitFrameFence()
{
    auto sync = presentSync_;

    {
        std::lock_guard<std::mutex> lock(sync->mutex);
        ++sync->inFlight;
    }

    // The fence completion runs on the queue's render thread (or inline where
    // the backend has no GPU), decrements off the loop thread, and wakes an
    // acquire() waiting for a slot. Capturing the shared state, not the window,
    // keeps it safe if the window is torn down with a fence still pending.
    CommandBuffer commandBuffer;
    commandBuffer.pushFence([sync]
        {
            std::lock_guard<std::mutex> lock(sync->mutex);
            --sync->inFlight;
            sync->slotFreed.notify_all();
        });

    getMainRenderQueue().submit(std::move(commandBuffer));
}

} // namespace ase
