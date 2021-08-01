#pragma once

#include "ondrawcustom.h"
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
#include <avg/rendertree.h>

#include <chrono>

namespace reactive::widget
{
    namespace detail
    {
        /*
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

        inline avg::RenderTree buildTree(avg::UniqueId id,
                avg::Obb const& obb, float radius,
                Theme const& theme, avg::Color color)
        {
            auto pen = avg::Pen(avg::Brush(color),
                    1.0f);
            auto brush = avg::Brush(theme.getBackground());

            return avg::RenderTree(std::make_shared<avg::RectNode>(
                    id,
                    obb.shrink(2.5f),
                    avg::TransitionOptions {},
                    radius,
                    btl::just(brush),
                    btl::just(pen)
                    ));
        }
        */
    } // namespace detail

    template <typename T, typename U>
    auto frameFull(
            Signal<T, float> cornerRadius,
            Signal<U, avg::Color> color
            )
    {
        auto cr = signal::share(std::move(cornerRadius));
        auto c = signal::share(std::move(color));

        return
            /*
            margin(signal::constant(5.0f))
            >> mapFactoryWidget(
                    makeWidgetTransformer()
                    .compose(bindDrawContext(), bindSize(), bindTheme())
                    .values(cr, c)
                    .bind(onDrawBehind(&detail::drawFrame))
                    )
            >> mapFactoryWidget(
                    makeWidgetTransformer()
                    .compose(bindObb(), bindTheme())
                    .values(cr, c)
                    .bind([id](auto obb, auto theme, auto radius, auto color)
                        {
                            auto tree = signal::map(detail::buildTree,
                                    signal::constant(id),
                                    std::move(obb),
                                    std::move(radius),
                                    std::move(theme),
                                    std::move(color)
                                    );

                            return makeWidgetTransformer([tree=btl::cloneOnCopy(std::move(tree))]
                                (auto w) mutable
                                {
                                    return makeWidgetTransformerResult(
                                            std::move(w).setRenderTree(std::move(*tree))
                                            );
                                });
                        })
                    )
            >>*/ mapFactoryWidget(
                    makeWidgetTransformer()
                    .compose(bindTheme())
                    .values(cr, c)
                    .bind(onDrawBehindCustom([](avg::DrawContext const& context,
                                avg::Vector2f size, Theme const& theme, float radius,
                                avg::Color const& color)
                        {
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
                                + avg::Shape(path, btl::just(brush), btl::just(pen))
                                ;
                        }
                        )
                    )
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

