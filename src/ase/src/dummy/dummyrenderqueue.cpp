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
    // Nothing draws, but fences must still complete: the frame loop tracks GPU
    // frames in flight by a fence per submit, so leaving them pending would
    // wedge its backpressure.
    for (auto& command : commandBuffer)
        if (std::holds_alternative<FenceCommand>(command))
            std::get<FenceCommand>(command).promise.set();

    if (recording_)
        submissions_.push_back(std::move(commandBuffer));
}

void DummyRenderQueue::recordSubmissions(bool record)
{
    recording_ = record;
}

std::vector<CommandBuffer> const& DummyRenderQueue::getSubmissions() const
{
    return submissions_;
}

} // namespace
