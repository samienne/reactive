#include "dummyrenderqueue.h"

#include "commandbuffer.h"
#include "rendercommand.h"

#include <variant>

namespace ase
{

DummyRenderQueue::DummyRenderQueue()
{
}

void DummyRenderQueue::flush()
{
}

void DummyRenderQueue::finish()
{
}

void DummyRenderQueue::submit(CommandBuffer&& commandBuffer)
{
    // Nothing draws, but fences must still complete: the frame loop tracks GPU
    // frames in flight by a fence per submit, so leaving them pending would wedge
    // its backpressure. With no GPU to wait on, a submitted frame is done at once.
    for (auto& command : commandBuffer)
    {
        if (std::holds_alternative<FenceCommand>(command))
            std::get<FenceCommand>(command).promise.set();
    }
}

} // namespace

