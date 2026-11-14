#include "glrenderqueue.h"

#include "commandbuffer.h"
#include "gldispatchedcontext.h"

#include <tracy/Tracy.hpp>

namespace ase
{

GlRenderQueue::GlRenderQueue(std::shared_ptr<GlDispatchedContext> dispatcher) :
    dispatcher_(std::move(dispatcher)),
    renderState_(*dispatcher_)
{
}

GlRenderQueue::~GlRenderQueue()
{
}

void GlRenderQueue::dispatch(std::function<void(GlFunctions const&)> f)
{
    dispatcher_->dispatch(std::move(f));
}

GlDispatchedContext& GlRenderQueue::getDispatcher()
{
    return *dispatcher_;
}

GlDispatchedContext const& GlRenderQueue::getDispatcher() const
{
    return *dispatcher_;
}

void GlRenderQueue::flush()
{
    ZoneScoped;
    dispatcher_->wait();
}

void GlRenderQueue::finish()
{
    dispatcher_->dispatch([](GlFunctions const&)
            {
                glFinish();
            });

    flush();
}

void GlRenderQueue::submit(CommandBuffer&& renderQueue)
{
    renderState_.submit(std::move(renderQueue));
}

} // namespace ase

