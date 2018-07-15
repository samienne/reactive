#pragma once

#include "ondraw.h"

#include "reactive/rendering.h"
#include "reactive/signal.h"

#include <avg/vector.h>
#include <avg/brush.h>

namespace reactive::widget
{
    inline auto drawBackground(avg::Vector2f size, avg::Brush const brush)
        -> avg::Drawing
    {
        auto t = avg::Transform()
            .translate(0.5f * size[0], 0.5f * size[1]);

        return t * avg::Drawing(makeShape(
                    makeRect(size[0], size[1]),
                    btl::just(brush),
                    btl::none));
    }

    inline auto background(Signal<avg::Brush> brush)
        // -> FactoryMap;
    {
        return onDrawBehind<SizeTag>(
                    drawBackground, std::move(brush));
    }

    inline auto background()
        // -> FactoryMap;
    {
        return onDrawBehind<SizeTag, ThemeTag>(
                [](auto size, auto const& theme)
                {
                    return drawBackground(size, theme.getBackground());
                });
    }
} // namespace reactive::widget

