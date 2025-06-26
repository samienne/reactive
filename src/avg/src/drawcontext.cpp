#include "drawcontext.h"

namespace avg
{

DrawContext::DrawContext(Painter* painter) :
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

PathBuilder DrawContext::pathBuilder() const
{
    return PathBuilder(getResource());
}

} // namespace reactive

