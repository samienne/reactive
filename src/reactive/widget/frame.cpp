#include "widget/frame.h"

#include "widget/ondraw.h"
#include "widget/margin.h"
#include "widget/theme.h"
#include "widget/instance.h"
#include "widget/builder.h"

#include "reactive/animate.h"
#include "reactive/shapes.h"

#include "reactive/signal/constant.h"
#include "reactive/signal/signal.h"

#include <avg/transform.h>
#include <avg/rendertree.h>

#include <chrono>

namespace reactive::widget
{

AnyWidgetModifier frame(
        AnySignal<float> cornerRadius,
        AnySignal<avg::Color> color
        )
{
    auto cr = signal::share(animate(std::move(cornerRadius)));
    auto c = signal::share(animate(std::move(color)));

    return onDrawBehind(
            [](avg::DrawContext const& context, avg::Vector2f size,
                float radius, avg::Color const& color)
                {
                    widget::Theme theme;

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

                    auto pen = avg::Pen(avg::Brush(color),
                            1.0f);
                    auto brush = avg::Brush(theme.getBackground());

                    return avg::Shape(std::move(path))
                        .fillAndStroke(brush, pen)
                        ;
                },
                std::move(cr),
                std::move(c
                )
            );
}

AnyWidgetModifier frame(AnySignal<avg::Color> color)
{
    return frame(signal::constant(10.0f), std::move(color));
}

AnyWidgetModifier frame(AnySignal<float> cornerRadius)
{
    return frame(
            std::move(cornerRadius),
            signal::constant(widget::Theme().getBackgroundHighlight())
            );
}

AnyWidgetModifier frame()
{
    return frame(
            signal::constant(10.0f),
            signal::constant(widget::Theme().getBackgroundHighlight())
            );
}
} // reactive::widget

