#pragma once

#include "pathbuilder.h"
#include "vector.h"
#include "rect.h"

#include <pmr/vector.h>

namespace avg
{
    inline Rect calculateBounds(
            pmr::vector<PathBuilder::SegmentType> segments,
            pmr::vector<Vector2f> const& vec)
    {
        if (segments.empty())
            return Rect();

        Rect rect;
        auto i = vec.begin();
        Vector2f cur(0.0f, 0.0f);

        for (auto const& s : segments)
        {
            switch(s)
            {
            case Path::SEGMENT_START:
                cur = *(i++);
                rect = rect.include(cur);
                break;
            case Path::SEGMENT_LINE:
                cur = *(i++);
                rect = rect.include(cur);
                break;
            case Path::SEGMENT_CONIC:
                rect = rect.include(*(i++));
                cur = *(i++);
                rect = rect.include(cur);
                break;
            case Path::SEGMENT_CUBIC:
                rect = rect.include(*(i++));
                rect = rect.include(*(i++));
                cur = *(i++);
                rect = rect.include(cur);
                break;
            case Path::SEGMENT_ARC:
                {
                    Vector2f c = *(i++);
                    Vector2f p = *(i++);
                    Vector2f d = cur - c;
                    float r = sqrt(d[0]*d[0]+d[1]*d[1]);

                    rect = rect.include(Vector2f(c[0]-r, c[1]-r));
                    rect = rect.include(Vector2f(c[0]+r, c[1]-r));
                    rect = rect.include(Vector2f(c[0]+r, c[1]+r));
                    rect = rect.include(Vector2f(c[0]-r, c[1]+r));

                    break;
                }
            }
        }

        return rect;
    }
} // namespace avg

