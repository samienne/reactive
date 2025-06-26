#include "bqui/modifier/frame.h"

#include "bqui/modifier/margin.h"
#include "bqui/modifier/background.h"

#include "bqui/shape/rectangle.h"

#include "bqui/theme.h"

namespace bqui::modifier
{

AnyWidgetModifier frame(
        bq::signal::AnySignal<float> cornerRadius,
        bq::signal::AnySignal<avg::Color> color
        )
{
    auto pen = std::move(color).map([](auto const& color)
        {
            return avg::Pen(avg::Brush(color));
        });

    return background(
            shape::rectangle(std::move(cornerRadius))
            .stroke(std::move(pen))
            | margin(2.5f)
            );
}

AnyWidgetModifier frame(bq::signal::AnySignal<avg::Color> color)
{
    return frame(bq::signal::constant(10.0f), std::move(color));
}

AnyWidgetModifier frame(bq::signal::AnySignal<float> cornerRadius)
{
    return frame(
            std::move(cornerRadius),
            bq::signal::constant(Theme().getBackgroundHighlight())
            );
}

AnyWidgetModifier frame()
{
    return frame(
            bq::signal::constant(10.0f),
            bq::signal::constant(Theme().getBackgroundHighlight())
            );
}
}

