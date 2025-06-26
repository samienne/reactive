#include "dummyrenderqueue.h"

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

} // namespace

