#include "simplepolygon.h"

namespace avg
{

SimplePolygon::SimplePolygon(pmr::vector<Vector2i>&& vertices) :
    vertices_(std::move(vertices))
{
}

SimplePolygon::~SimplePolygon()
{
}

pmr::vector<Vector2i> const& SimplePolygon::getVertices() const
{
    return vertices_;
}

} // namespace

