#include "dummyrenderqueue.h"

#include "basicfence.h"
#include "commandbuffer.h"
#include "rendercommand.h"

#include <exception>
#include <variant>

namespace ase
{

DummyRenderQueue::DummyRenderQueue() :
    drain_(std::make_shared<FenceDrain>())
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
    std::vector<btl::future::Promise<>> signaled;

    for (auto& command : commandBuffer)
    {
        if (std::holds_alternative<FenceCommand>(command))
        {
            std::get<FenceCommand>(command).fence.getImpl<BasicFence>()
                .signal(signaled);
        }
        else if (auto* readback = std::get_if<ReadbackCommand>(&command))
        {
            readback->promise.setFailure(std::make_exception_ptr(
                        ReadbackFailure(ReadbackError::Unsupported)));
        }
    }

    drain_->post(std::move(signaled));
}

} // namespace
