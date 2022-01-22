#pragma once

#include "widget.h"

namespace reactive::widget
{
    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyWidget, TFunc, AnyWidget, BuildParams const&, Ts&&...>
        >
    >
    auto withParamsObject(TFunc&& func, Ts&&... ts)
    {
        return detail::makeWidgetModifierUnchecked(
            [](auto widget, auto&& func, auto&&... ts)
            {
                return detail::makeWidgetUnchecked(
                        [](BuildParams params, auto&& widget, auto&& func, auto&&... ts)
                        // -> AnyBuilder
                        {
                            return std::invoke(
                                    std::invoke(
                                        std::forward<decltype(func)>(func),
                                        std::forward<decltype(widget)>(widget),
                                        params,
                                        std::forward<decltype(ts)>(ts)...
                                        ),
                                    std::move(params)
                                    );
                        },
                        std::move(widget),
                        std::forward<decltype(func)>(func),
                        std::forward<decltype(ts)>(ts)...
                        );
            },
            std::forward<TFunc>(func),
            std::forward<Ts>(ts)...
            );
    }
} // namespace reactive::widget

