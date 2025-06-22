#pragma once

#include "path.h"
#include "pathbuilder.h"
#include "vector.h"
#include "rect.h"

#include <pmr/vector.h>

namespace avg
{
    inline Rect calculateBounds(Path const& path)
    {
        Rect rect;
        Vector2f cur(0.0f, 0.0f);

        for (auto i = path.begin(); i != path.end(); ++i)
        {
            switch(i.getType())
            {
            case Path::SegmentType::start:
                cur = i.getStart().v;
                rect = rect.include(cur);
                break;
            case Path::SegmentType::line:
                cur = i.getLine().v;
                rect = rect.include(cur);
                break;
            case Path::SegmentType::conic:
                rect = rect.include(i.getConic().v1);
                cur = i.getConic().v2;
                rect = rect.include(cur);
                break;
            case Path::SegmentType::cubic:
                rect = rect.include(i.getCubic().v1);
                rect = rect.include(i.getCubic().v2);
                cur = i.getCubic().v3;
                rect = rect.include(cur);
                break;
            case Path::SegmentType::arc:
                {
                    Vector2f c = i.getArc().center;
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

