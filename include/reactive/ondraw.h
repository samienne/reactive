#pragma once

#include "signaltraits.h"
#include "widgetmap.h"

#include <avg/drawing.h>

#include <type_traits>
#include <functional>

namespace reactive
{
    template <typename... Ts, typename TFunc, typename... Us,
             typename = std::enable_if_t
        <
            std::is_assignable<
                std::function<avg::Drawing(typename Ts::Type..., SignalType<Us>...)>,
                TFunc
            >::value
        >
    >
    auto onDraw(TFunc&& func, Us... us)
    {
        return makeWidgetMap<DrawingTag, ObbTag, Ts...>(
                [func=std::forward<TFunc>(func)]
                (auto d1, avg::Obb const& obb, auto&&... ts) -> avg::Drawing
                {
                    auto d2 = func(
                        std::forward<decltype(ts)>(ts)...
                        );

                    return std::move(d1) + (obb.getTransform() * std::move(d2));
                },
                std::move(us)...
                );
    }

    template <typename... Ts, typename TFunc, typename... Us,
             typename = std::enable_if_t
        <
            std::is_assignable<
                std::function<avg::Drawing(typename Ts::Type..., SignalType<Us>...)>,
                TFunc
            >::value
        >
    >
    auto onDrawBehind(TFunc&& func, Us... us)
    {
        return makeWidgetMap<DrawingTag, ObbTag, Ts...>(
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
} // namespace reactive

