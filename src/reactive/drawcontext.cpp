#include "drawcontext.h"

namespace reactive
{

DrawContext::DrawContext(avg::Painter* painter) :
    painter_(painter)

{
}

bool DrawContext::operator==(DrawContext const& other) const
{
    return painter_ == other.painter_;
}

pmr::memory_resource* DrawContext::getResource() const
{
    return painter_->getResource();
}

avg::PathBuilder DrawContext::pathBuilder() const
{
    return avg::PathBuilder(getResource());
}

} // namespace reactive

