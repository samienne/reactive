#pragma once

#include "margin.h"
#include "theme.h"
#include "ondraw.h"
#include "binddrawcontext.h"
#include "bindsize.h"
#include "bindtheme.h"
#include "widgettransformer.h"

#include "reactive/widget.h"
#include "reactive/widgetfactory.h"

#include "reactive/shapes.h"

#include "reactive/signal/constant.h"
#include "reactive/signal/signal.h"

#include <avg/transform.h>

namespace reactive::widget
{
    namespace detail
    {
        inline avg::Drawing drawFrame(avg::DrawContext drawContext,
                avg::Vector2f size, widget::Theme const& theme,
                float cornerRadius, avg::Color const& color
                )
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

            return avg::translate(0.5f * size[0], 0.5f * size[1])
                * drawContext.drawing(std::move(shape));
        }
    } // namespace detail

    template <typename T, typename U>
    auto frameFull(
            Signal<T, float> cornerRadius,
            Signal<U, avg::Color> color
            )
    {

        return
            margin(signal::constant(5.0f))
            >> mapFactoryWidget(
                    makeWidgetTransformer()
                    .compose(bindDrawContext(), bindSize(), bindTheme())
                    .values(std::move(cornerRadius), std::move(color))
                    .bind(onDrawBehind(&detail::drawFrame))
                    )
            ;
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

