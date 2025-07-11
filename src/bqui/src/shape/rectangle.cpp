#include "bqui/shape/rectangle.h"

#include "bqui/shape/shape.h"

#include "bqui/animate.h"

#include <avg/brush.h>
#include <optional>

namespace bqui::shape
{

namespace
{
    avg::Shape makeRectangle(
            avg::DrawContext const& context,
            avg::Vector2f size,
            float radius)
    {
        radius = std::clamp(radius, 0.0f, std::min(size[0], size[1]) / 2.0f);

        using avg::Vector2f;

        float x1 = 0;
        float y1 = 0;
        float x2 = size[0];
        float y2 = size[1];

        return context.pathBuilder()
            .start(Vector2f(radius, y1))
            .lineTo(x2 - radius, y1)
            .conicTo(Vector2f(x2, y1), Vector2f(x2, y1 + radius))
            .lineTo(x2, y2 - radius)
            .conicTo(Vector2f(x2, y2), Vector2f(x2 - radius, y2))
            .lineTo(x1 + radius, y2)
            .conicTo(Vector2f(x1, y2), Vector2f(x1, y2 - radius))
            .lineTo(x1, y1 + radius)
            .conicTo(Vector2f(x1, y1), Vector2f(x1 + radius, y1))
            .build()
            ;
    }
} // anonymous namespace

AnyShape rectangle()
{
    return rectangle(bq::signal::constant(5.0f));
}

AnyShape rectangle(bq::signal::AnySignal<float> cornerRadius)
{
    return shape::makeShape(makeRectangle, animate(std::move(cornerRadius)));
}

}
