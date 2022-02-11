#pragma once

#include "ondraw.h"
#include "margin.h"
#include "theme.h"
#include "instance.h"
#include "builder.h"

#include "reactive/shapes.h"

#include "reactive/signal/constant.h"
#include "reactive/signal/signal.h"

#include <avg/transform.h>
#include <avg/rendertree.h>

#include <chrono>

namespace reactive::widget
{
    template <typename T, typename U>
    auto frameFull(
            Signal<T, float> cornerRadius,
            Signal<U, avg::Color> color
            )
    {
        auto cr = signal::share(std::move(cornerRadius));
        auto c = signal::share(std::move(color));

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

                        return context.drawing()
                            + avg::Shape(path, std::make_optional(brush), std::make_optional(pen))
                            ;
                    },
                    std::move(cr),
                    std::move(c
                    )
                );
    }

    template <typename T>
    inline auto frame(Signal<T, avg::Color> color)
    {
        return frameFull(signal::constant(10.0f), std::move(color));
    }

    template <typename T>
    inline auto frame(Signal<T, float> cornerRadius)
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

