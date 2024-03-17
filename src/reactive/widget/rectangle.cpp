#include "widget/rectangle.h"

#include "widget/ondraw.h"
#include "widget/providetheme.h"
#include "widget/setsizehint.h"

#include "animate.h"

#include <avg/brush.h>
#include <optional>

namespace reactive::widget
{

namespace
{
    avg::Drawing drawRectangle(
            avg::DrawContext const& context,
            avg::Vector2f size,
            float radius,
            std::optional<avg::Brush> const& brush,
            std::optional<avg::Pen> const& pen)
    {
        radius = std::clamp(radius, 0.0f, std::min(size[0], size[1]) / 2.0f);

        using avg::Vector2f;

        float x1 = 2.5f;
        float y1 = 2.5f;
        float x2 = size[0] - 2.5f;
        float y2 = size[1] - 2.5f;

        auto path = context.pathBuilder()
            .start(Vector2f(radius, y1))
            .lineTo(x2 - radius, y1)
            .conicTo(Vector2f(x2, y1), Vector2f(x2, y1 + radius))
            .lineTo(x2, y2 - radius)
            .conicTo(Vector2f(x2, y2), Vector2f(x2 - radius, y2))
            .lineTo(x1 + radius, y2)
            .conicTo(Vector2f(x1, y2), Vector2f(x1, y2 - radius))
            .lineTo(x1, y1 + radius)
            .conicTo(Vector2f(x1, y1), Vector2f(x1 + radius, y1))
            .build();

        return context.drawing()
            + avg::Shape(std::move(path), std::move(brush), std::move(pen))
            ;
    }

    template <typename T>
    AnySignal<std::optional<T>> flipOptional(std::optional<AnySignal<T>> s)
    {
        if (!s)
            return signal::constant<std::optional<T>>(std::nullopt);

        return std::move(*s).map([](auto value)
            {
                return std::make_optional(std::move(value));
            });
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
    return makeWidget()
        | onDraw(drawRectangle,
                std::move(cornerRadius),
                animate(flipOptional(std::move(brush))),
                animate(flipOptional(std::move(pen)))
                )
        | setSizeHint(signal::constant(simpleSizeHint(1, 1)))
        ;
}
} // namespace reactive::widget
