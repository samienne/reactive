#pragma once

#include "vector.h"

#include <vector>

namespace avg
{
    class SimplePolygon
    {
    public:
        SimplePolygon();
        SimplePolygon(std::vector<Vector2i>&& vertices);
        ~SimplePolygon();

        std::vector<Vector2i> const& getVertices() const;

    private:
        std::vector<Vector2i> vertices_;
    };
}

