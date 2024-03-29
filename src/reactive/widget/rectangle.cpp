#include "widget/rectangle.h"

#include "shape/shape.h"
#include "shape/rectangle.h"

#include "widget/providetheme.h"
#include "widget/setsizehint.h"
#include "widget/margin.h"

#include "animate.h"

#include <avg/brush.h>
#include <optional>

namespace reactive::widget
{

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
                    std::move(brush),
                    std::move(pen)
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

    return shape::rectangle(std::move(cornerRadius))
        .fillAndStroke(std::move(brush), std::move(pen))
        | widget::margin(std::move(margin))
        | setSizeHint(signal::constant(simpleSizeHint(1, 1)))
        ;
}
} // namespace reactive::widget
