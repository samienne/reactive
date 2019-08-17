#include "drawcontext.h"

namespace reactive
{

DrawContext::DrawContext(pmr::memory_resource* memory) :
    memory_(memory)

{
}

pmr::memory_resource* DrawContext::getResource() const
{
    return memory_;
}

avg::PathBuilder DrawContext::pathBuilder() const
{
    return avg::PathBuilder(memory_);
}

} // namespace reactive

