#pragma once

#include "binddrawing.h"
#include "bindobb.h"
#include "setdrawing.h"

#include "reactive/bindwidgetmap.h"
#include "reactive/signaltraits.h"
#include "reactive/widgetmap.h"

#include <avg/drawing.h>

#include <type_traits>
#include <functional>

namespace reactive::widget
{
    namespace detail
    {
        template <typename TFunc, typename... Us,
                typename = std::enable_if_t
            <
            std::is_invocable_r_v<avg::Drawing, TFunc, SignalType<Us>...>
            >
        >
        auto onDraw(TFunc&& func, Us... us)
        {
            return makeWidgetMap()
                .provide(grabDrawing(), bindObb())
                .provideValues(std::move(us)...)
                .bindWidgetMap([func=std::forward<TFunc>(func)]
                (auto drawing, auto obb, auto... us) mutable
                {
                    auto newDrawing = signal::map(
                        [func=std::move(func)]
                        (auto d1, avg::Obb const& obb, auto&&... ts) -> avg::Drawing
                        {
                            auto d2 = func(
                                std::forward<decltype(ts)>(ts)...
                                );

                            return std::move(d1) + (obb.getTransform() * std::move(d2));
                        },
                        std::move(drawing),
                        std::move(obb),
                        std::move(us)...
                        );

                    return setDrawing(std::move(newDrawing));
                });
        }

        template <typename TFunc, typename... Us,
                typename = std::enable_if_t
            <
            std::is_invocable_r_v<avg::Drawing, TFunc, SignalType<Us>...>
            >
        >
        auto onDrawBehind(TFunc&& func, Us... us)
        {
            return makeWidgetMap<DrawingTag, ObbTag>(
                    [func=std::forward<TFunc>(func)]
                    (auto d1, avg::Obb const& obb, auto&&... ts) -> avg::Drawing
                    {
                        auto d2 = func(
                            std::forward<decltype(ts)>(ts)...
                            );

                        return (obb.getTransform() * std::move(d2)) + std::move(d1);
                    },
                    std::move(us)...
                    );
        }
    } // namespace detail

    template <typename TFunc>
    auto onDraw(TFunc&& f)
    {
        return bindWidgetMap([f=std::forward<TFunc>(f)](auto... values) mutable
                /*-> decltype(
                    detail::onDraw(std::move(f), std::move(values)...)
                    )*/
        {
            return detail::onDraw(std::move(f), std::move(values)...);
        });
    }

    template <typename TFunc>
    auto onDrawBehind(TFunc&& f)
    {
        return bindWidgetMap([f=std::forward<TFunc>(f)](auto... values) mutable
                /*-> decltype(
                    detail::onDrawBehind(std::move(f), std::move(values)...)
                    )*/
        {
            return detail::onDrawBehind(std::move(f), std::move(values)...);
        });
    }

} // namespace reactive::widget

