#include "widget/rectangle.h"

#include "shape/shape.h"

#include "widget/providetheme.h"
#include "widget/setsizehint.h"
#include "widget/margin.h"

#include "animate.h"

#include <avg/brush.h>
#include <optional>

namespace reactive::widget
{

namespace
{
    avg::Shape drawRectangle(
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

REACTIVE_EXPORT AnyWidget rectangle()
{
    return makeWidget([](auto theme)
        {
            auto brush = theme.clone().map([](Theme const& theme)
                {
                    return avg::Brush(theme.getBackground());
                });

            auto pen = theme.clone().map([](Theme const& theme)
                {
                    return avg::Pen(avg::Brush(theme.getEmphasized()));
                });

            return rectangle(
                    signal::constant(5.0f),
                    std::make_optional(std::move(brush)),
                    std::make_optional(std::move(pen))
                    );
        },
        provideTheme()
        );
}

REACTIVE_EXPORT AnyWidget rectangle(
        AnySignal<float> cornerRadius,
        std::optional<AnySignal<avg::Brush>> brush,
        std::optional<AnySignal<avg::Pen>> pen
        )
{
    AnySignal<float> margin = signal::constant(0.0f);
    if (pen) {
        margin = pen->clone().map([](avg::Pen const& pen)
                {
                    return pen.getWidth() / 2.0f;
                });
    }

    return shape::makeShape(drawRectangle, animate(std::move(cornerRadius)))
        .fillAndStroke(std::move(brush), std::move(pen))
        | widget::margin(2.5f)
        | setSizeHint(signal::constant(simpleSizeHint(1, 1)))
        ;
}
} // namespace reactive::widget
