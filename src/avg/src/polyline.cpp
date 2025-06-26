#include "polyline.h"

namespace avg
{

PolyLine::PolyLine(pmr::vector<Vector2i>&& vertices) :
    vertices_(std::move(vertices))
{
}

PolyLine::~PolyLine()
{
}

pmr::vector<Vector2i> const& PolyLine::getVertices() const
{
    return vertices_;
}

} // namespace

