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
    {
        if (auto* fence = std::get_if<FenceCommand>(&command))
        {
            fence->promise.set();
        }
        else if (auto* readback = std::get_if<ReadbackCommand>(&command))
        {
            readback->callback(ReadbackError::Unsupported);
        }
    }
}

} // namespace
