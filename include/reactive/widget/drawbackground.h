#pragma once

#include "ondraw.h"

#include "reactive/shapes.h"
#include "reactive/signal.h"

#include <avg/drawing.h>
#include <avg/vector.h>
#include <avg/brush.h>

namespace reactive::widget
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

    inline auto background(Signal<avg::Brush> brush)
        // -> FactoryMap;
    {
        return onDrawBehind<DrawContextTag, SizeTag>(
                    drawBackground, std::move(brush));
    }

    inline auto background()
        // -> FactoryMap;
    {
        return onDrawBehind<DrawContextTag, SizeTag, ThemeTag>(
            [](DrawContext const& drawContext, auto size, auto const& theme)
            {
                return drawBackground(drawContext, size, theme.getBackground());
            });
    }
} // namespace reactive::widget

