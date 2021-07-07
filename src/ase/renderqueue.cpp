#include "renderqueue.h"
#include "renderqueueimpl.h"

namespace ase
{

RenderQueue::RenderQueue(std::shared_ptr<RenderQueueImpl> impl) :
    deferred_(std::move(impl))
{
}

void RenderQueue::flush()
{
    d()->flush();
}

void RenderQueue::finish()
{
    d()->finish();
}

void RenderQueue::submit(CommandBuffer&& commandBuffer)
{
    d()->submit(std::move(commandBuffer));
}

} // namespace ase

