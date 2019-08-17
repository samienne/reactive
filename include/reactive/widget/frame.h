#pragma once

#include "margin.h"
#include "theme.h"

#include "reactive/widget.h"
#include "reactive/widgetfactory.h"

#include "reactive/rendering.h"

#include "reactive/signal/constant.h"
#include "reactive/signal.h"

#include <avg/transform.h>

namespace reactive::widget
{
    template <typename T, typename U>
    auto frameFull(
            Signal<float, T> cornerRadius,
            Signal<avg::Color, U> color
            )
    {
        auto f = [](DrawContext drawContext,
                avg::Vector2f size, widget::Theme const& theme,
                float cornerRadius, avg::Color const& color
                ) -> avg::Drawing
        {
            auto pen = avg::Pen(avg::Brush(color),
                    1.0f);
            auto brush = avg::Brush(theme.getBackground());

            auto shape =  makeShape(
                    makeRoundedRect(drawContext.getResource(),
                        size[0] - 5.0f, size[1] - 5.0f,
                        cornerRadius),
                    btl::just(brush),
                    btl::just(pen)
                    );

            return avg::Transform().translate(0.5f * size[0], 0.5f * size[1])
                * drawContext.drawing(std::move(shape));
        };

        return
            margin(signal::constant(5.0f))
            >> mapFactoryWidget(
                    onDrawBehind<DrawContextTag, SizeTag, ThemeTag>(
                        std::move(f),
                        std::move(cornerRadius),
                        std::move(color)
                        )
                    )
            ;
    }


    template <typename T>
    inline auto frame(Signal<avg::Color, T> color)
    {
        return frameFull(signal::constant(10.0f), std::move(color));
    }

    template <typename T>
    inline auto frame(Signal<float, T> cornerRadius)
    {
        return frameFull(
                std::move(cornerRadius),
                signal::constant(widget::Theme().getBackgroundHighlight())
                );
    }

    inline auto frame()
    {
        return frameFull(
                signal::constant(10.0f),
                signal::constant(widget::Theme().getBackgroundHighlight())
                );
    }
} // namespace reactive::widget

