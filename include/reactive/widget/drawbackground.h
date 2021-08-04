#pragma once

#include "ondrawcustom.h"
#include "bindsize.h"
#include "binddrawcontext.h"
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
        return makeWidgetTransformer()
            .values(std::move(brush))
            .bind(onDrawBehindCustom(detail::drawBackground))
            ;
    }

    inline auto background()
    {
        return makeWidgetTransformer()
            .compose(bindTheme())
            .bind([](auto theme)
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

