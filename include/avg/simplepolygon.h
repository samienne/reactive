#pragma once

#include <ase/vector.h>

#include <vector>

namespace avg
{
    class SimplePolygon
    {
    public:
        SimplePolygon();
        SimplePolygon(std::vector<ase::Vector2i>&& vertices);
        ~SimplePolygon();

        std::vector<ase::Vector2i> const& getVertices() const;

    private:
        std::vector<ase::Vector2i> vertices_;
    };
}

