#pragma once

#include "vector.h"
#include "avgvisibility.h"

#include <vector>

namespace avg
{
    class AVG_EXPORT SimplePolygon
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

