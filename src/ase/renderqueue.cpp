#include "renderqueue.h"
#include "renderqueueimpl.h"

#include <tracy/Tracy.hpp>

namespace ase
{

RenderQueue::RenderQueue(std::shared_ptr<RenderQueueImpl> impl) :
    deferred_(std::move(impl))
{
}

void RenderQueue::flush()
{
    ZoneScoped;
    d()->flush();
}

void RenderQueue::finish()
{
    ZoneScoped;
    d()->finish();
}

void RenderQueue::submit(CommandBuffer&& commandBuffer)
{
    ZoneScoped;
    d()->submit(std::move(commandBuffer));
}

} // namespace ase

