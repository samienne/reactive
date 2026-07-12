#include "bqui/shape/circle.h"

#include "bqui/shape/shape.h"

#include <avg/vector.h>

#include <algorithm>

namespace bqui::shape
{

namespace
{
    avg::Shape makeCircle(
            avg::DrawContext const& context,
            avg::Vector2f size)
    {
        using avg::Vector2f;

        float r = std::min(size[0], size[1]) / 2.0f;
        float cx = size[0] / 2.0f;
        float cy = size[1] / 2.0f;
        float x1 = cx - r;
        float y1 = cy - r;
        float x2 = cx + r;
        float y2 = cy + r;

        // Four quarter-arc conics around the centered [x1,y1]-[x2,y2] square.
        return context.pathBuilder()
            .start(Vector2f(x2, cy))
            .conicTo(Vector2f(x2, y2), Vector2f(cx, y2))
            .conicTo(Vector2f(x1, y2), Vector2f(x1, cy))
            .conicTo(Vector2f(x1, y1), Vector2f(cx, y1))
            .conicTo(Vector2f(x2, y1), Vector2f(x2, cy))
            .build()
            ;
    }
} // anonymous namespace

AnyShape circle()
{
    return shape::makeShape(makeCircle);
}

}
