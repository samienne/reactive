#include "renderqueue.h"

#include "rendercommand.h"

namespace ase
{

void RenderQueue::push(RenderCommand&& command)
{
    commands_.push_back(std::move(command));
}

} // namespace ase

