#pragma once

#include "ondraw.h"
#include "bindsize.h"
#include "bindtheme.h"

#include "reactive/shapes.h"

#include "reactive/signal/signal.h"

#include <avg/drawing.h>
#include <avg/vector.h>
#include <avg/brush.h>

namespace reactive::widget
{
    namespace detail
    {
        inline auto drawBackground(avg::DrawContext const& drawContext,
                avg::Vector2f size, avg::Brush const& brush)
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
    auto background(Signal<T, avg::Brush> brush)
    {
        return onDrawBehind(detail::drawBackground, std::move(brush));
    }

    inline auto background()
    {
        widget::Theme theme;

        return background(signal::constant(avg::Brush(theme.getBackground())));
    }
} // namespace reactive::widget

