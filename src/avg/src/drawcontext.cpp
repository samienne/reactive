#include "drawcontext.h"

#include "painter.h"

namespace avg
{

DrawContext::DrawContext(Painter* painter) :
    memory_(painter->getResource())
{
}

DrawContext::DrawContext(pmr::memory_resource* memory) :
    memory_(memory)
{
}

bool DrawContext::operator==(DrawContext const& other) const
{
    return memory_ == other.memory_;
}

pmr::memory_resource* DrawContext::getResource() const
{
    return memory_;
}

PathBuilder DrawContext::pathBuilder() const
{
    return PathBuilder(memory_);
}

} // namespace avg

