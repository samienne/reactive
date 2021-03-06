#pragma once

#include "binddrawing.h"
#include "bindobb.h"
#include "setdrawing.h"
#include "widgettransformer.h"

#include "reactive/signal/signaltraits.h"

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
            std::is_invocable_r_v<avg::Drawing, TFunc, signal::SignalType<Us>...>
            >
        >
        auto onDraw(TFunc&& func, Us... us)
        {
            return makeWidgetTransformer()
                .compose(grabDrawing(), bindObb())
                .values(std::move(us)...)
                .bind([func=std::forward<TFunc>(func)]
                (auto... values) mutable
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
                        std::move(values)...
                        );

                    return setDrawing(std::move(newDrawing));
                });
        }

        template <typename TFunc, typename... Us,
                typename = std::enable_if_t
            <
            std::is_invocable_r_v<avg::Drawing, TFunc, signal::SignalType<Us>...>
            >
        >
        auto onDrawBehind(TFunc&& func, Us... us)
        {
            return makeWidgetTransformer()
                .compose(grabDrawing(), bindObb())
                .values(std::move(us)...)
                .bind([func=std::forward<TFunc>(func)]
                (auto... values) mutable
                {
                    auto newDrawing = signal::map(
                        [func=std::forward<TFunc>(func)]
                        (auto d1, avg::Obb const& obb, auto&&... ts) -> avg::Drawing
                        {
                            auto d2 = func(
                                std::forward<decltype(ts)>(ts)...
                                );

                            return (obb.getTransform() * std::move(d2)) + std::move(d1);
                        },
                        std::move(values)...
                        );

                    return setDrawing(std::move(newDrawing));
                });
        }
    } // namespace detail

    template <typename TFunc>
    auto onDraw(TFunc&& f)
    {
        return [f=std::forward<TFunc>(f)](auto&&... values) mutable
        {
            return detail::onDraw(
                    std::move(f),
                    std::forward<decltype(values)>(values)...
                    );

        };
    }

    template <typename TFunc>
    auto onDrawBehind(TFunc&& f)
    {
        return [f=std::forward<TFunc>(f)](auto&&... values) mutable
        {
            return detail::onDrawBehind(
                    std::move(f),
                    std::forward<decltype(values)>(values)...
                    );

        };
    }

} // namespace reactive::widget

