#pragma once

#include "margin.h"
#include "theme.h"

#include "reactive/widget.h"
#include "reactive/widgetfactory.h"

#include "reactive/rendering.h"

#include "reactive/signal/constant.h"
#include "reactive/signal.h"

#include <avg/transform.h>

#include <iostream>

namespace reactive::widget
{
    template <typename T>
    auto frame(Signal<float, T> cornerRadius)
    {
        auto f = [](avg::Vector2f size, widget::Theme const& theme,
                float cornerRadius)
            -> avg::Drawing
        {
            auto pen = avg::Pen(avg::Brush(theme.getBackgroundHighlight()),
                    1.0f);

            auto brush = avg::Brush(theme.getBackground());

            auto shape =  makeShape(
                    makeRoundedRect(size[0] - 5.0f, size[1] - 5.0f,
                        cornerRadius),
                    btl::none,
                    btl::just(pen));

            return avg::Transform()
                .translate(0.5f * size[0],
                        0.5f * size[1])
                * avg::Drawing(shape);
        };

        return
            margin(signal::constant(5.0f))
            >> mapFactoryWidget(onDrawBehind<SizeTag, ThemeTag>(
                        std::move(f), std::move(cornerRadius)))
            ;
    }

    inline auto frame()
    {
        return frame(signal::constant(10.0f));
    }
} // namespace reactive::widget

