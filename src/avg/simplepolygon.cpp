#include "simplepolygon.h"

namespace avg
{

SimplePolygon::SimplePolygon()
{
}

SimplePolygon::SimplePolygon(std::vector<ase::Vector2i>&& vertices) :
    vertices_(std::move(vertices))
{
}

SimplePolygon::~SimplePolygon()
{
}

std::vector<ase::Vector2i> const& SimplePolygon::getVertices() const
{
    return vertices_;
}

} // namespace

