#include "drawcontext.h"

namespace reactive
{

DrawContext::DrawContext(std::shared_ptr<avg::Painter> painter) :
    painter_(std::move(painter))

{
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

