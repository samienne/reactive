#include "dummyrenderqueue.h"

#include "commandbuffer.h"
#include "rendercommand.h"

#include <variant>

namespace ase
{

void DummyRenderQueue::flush()
{
}

void DummyRenderQueue::finish()
{
}

void DummyRenderQueue::submit(CommandBuffer&& commandBuffer)
{
    // Nothing draws, but fences must still complete unless held: the frame
    // loop tracks GPU frames in flight by a fence per submit, so leaving them
    // pending wedges its backpressure.
    for (auto& command : commandBuffer)
    {
        if (!std::holds_alternative<FenceCommand>(command))
            continue;

        auto& promise = std::get<FenceCommand>(command).promise;
        if (holding_)
            held_.push_back(std::move(promise));
        else
            promise.set();
    }
}

void DummyRenderQueue::holdFences()
{
    holding_ = true;
}

void DummyRenderQueue::releaseFences()
{
    holding_ = false;

    auto held = std::move(held_);
    held_.clear();

    for (auto& promise : held)
        promise.set();
}

} // namespace
