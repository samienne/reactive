#include "widget/frame.h"

#include "widget/margin.h"
#include "widget/theme.h"
#include "widget/background.h"

#include "shape/rectangle.h"

namespace reactive::widget
{

AnyWidgetModifier frame(
        signal2::AnySignal<float> cornerRadius,
        signal2::AnySignal<avg::Color> color
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

AnyWidgetModifier frame(signal2::AnySignal<avg::Color> color)
{
    return frame(signal2::constant(10.0f), std::move(color));
}

AnyWidgetModifier frame(signal2::AnySignal<float> cornerRadius)
{
    return frame(
            std::move(cornerRadius),
            signal2::constant(widget::Theme().getBackgroundHighlight())
            );
}

AnyWidgetModifier frame()
{
    return frame(
            signal2::constant(10.0f),
            signal2::constant(widget::Theme().getBackgroundHighlight())
            );
}
} // reactive::widget

