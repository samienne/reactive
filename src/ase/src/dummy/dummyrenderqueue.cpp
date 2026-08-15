#include "dummyrenderqueue.h"

#include "window.h"
#include "dispatcher.h"

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

void DummyRenderQueue::submit(CommandBuffer&& /*renderQueue*/)
{
}

PresentStatus DummyRenderQueue::present(Window& window)
{
    return window.present(Dispatched());
}

} // namespace

