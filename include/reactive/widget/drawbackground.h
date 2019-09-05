#pragma once

#include "ondraw.h"
#include "bindsize.h"
#include "binddrawcontext.h"
#include "bindtheme.h"

#include "reactive/shapes.h"
#include "reactive/signal.h"

#include <avg/drawing.h>
#include <avg/vector.h>
#include <avg/brush.h>

namespace reactive::widget
{
    namespace detail
    {
        inline auto drawBackground(DrawContext const& drawContext,
                avg::Vector2f size, avg::Brush const brush)
            -> avg::Drawing
        {
            auto t = avg::Transform()
                .translate(0.5f * size[0], 0.5f * size[1]);

            return t * drawContext.drawing(makeShape(
                        makeRect(drawContext.getResource(), size[0], size[1]),
                        btl::just(brush),
                        btl::none
                        ));
        }
    } // namespace detail

    template <typename T>
    auto background(Signal<avg::Brush, T> brush)
    {
        return makeWidgetMap()
            .provide(bindDrawContext(), bindSize())
            .provideValues(std::move(brush))
            .bindWidgetMap([](auto drawContext, auto size, auto brush)
            {
                return onDrawBehind(detail::drawBackground, std::move(drawContext),
                            std::move(size), std::move(brush));
            });
    }

    inline auto background()
    {
        return makeWidgetMap()
            .provide(bindTheme())
            .bindWidgetMap([](auto theme)
            {
                return background(signal::map(
                            [](Theme const& theme)
                            {
                                return avg::Brush(theme.getBackground());
                            },
                            std::move(theme)
                            ));
            });
    }
} // namespace reactive::widget

