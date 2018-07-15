#pragma once

#include "vector.h"
#include "rect.h"

namespace avg
{
    inline Rect calculateBounds(std::vector<Vector2f> const& vec)
    {
        if (vec.empty())
            return Rect();

        float x1 = vec[0].x();
        float x2 = vec[0].x();
        float y1 = vec[0].y();
        float y2 = vec[0].y();

        for (auto const& v : vec)
        {
            x1 = std::min(x1, v.x());
            x2 = std::max(x2, v.x());
            y1 = std::min(y1, v.y());
            y2 = std::max(y2, v.y());
        }

        return Rect(Vector2f(x1, y1), Vector2f(x2-x1, y2-y1));
    }
} // namespace avg

