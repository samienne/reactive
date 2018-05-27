#pragma once

#include "vector.h"

#include <btl/visibility.h>

#include <vector>

namespace avg
{
    class BTL_VISIBLE SimplePolygon
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

