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
        if (auto* fenceCommand = std::get_if<FenceCommand>(&command))
        {
            if (holding_)
            {
                held_.push_back(std::move(fenceCommand->fence));
            }
            else
            {
                fenceCommand->fence.getImpl<BasicFence>().signal(signaled);
            }
        }
        else if (auto* readback = std::get_if<ReadbackCommand>(&command))
        {
            readback->promise.setFailure(std::make_exception_ptr(
                        ReadbackFailure(ReadbackError::Unsupported)));
        }
    }

    drain_->post(std::move(signaled));
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

    std::vector<btl::future::Promise<>> signaled;
    for (auto& fence : held)
        fence.getImpl<BasicFence>().signal(signaled);

    drain_->post(std::move(signaled));
}

} // namespace
