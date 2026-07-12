#include "bqui/shape/ellipse.h"

#include "bqui/shape/shape.h"

#include <avg/vector.h>

namespace bqui::shape
{

namespace
{
    avg::Shape makeEllipse(
            avg::DrawContext const& context,
            avg::Vector2f size)
    {
        using avg::Vector2f;

        float w = size[0];
        float h = size[1];
        float cx = w / 2.0f;
        float cy = h / 2.0f;

        // Four quarter-arc conics inscribing an ellipse in the [0,0]-[w,h] box,
        // with the box corners as the conic control points (same construction
        // as the rounded-rectangle corners).
        return context.pathBuilder()
            .start(Vector2f(w, cy))
            .conicTo(Vector2f(w, h), Vector2f(cx, h))
            .conicTo(Vector2f(0.0f, h), Vector2f(0.0f, cy))
            .conicTo(Vector2f(0.0f, 0.0f), Vector2f(cx, 0.0f))
            .conicTo(Vector2f(w, 0.0f), Vector2f(w, cy))
            .build()
            ;
    }
} // anonymous namespace

AnyShape ellipse()
{
    return shape::makeShape(makeEllipse);
}

}
