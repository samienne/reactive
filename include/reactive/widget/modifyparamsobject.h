#pragma once

#include "widget.h"

namespace reactive::widget
{
    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<BuildParams, TFunc, BuildParams, Ts&&...>
        >
    >
    auto modifyParamsObject(TFunc&& func, Ts&&... ts)
    {
        return detail::makeWidgetModifierUnchecked(
            [](auto widget, auto func, auto&&... ts)
            {
                return detail::makeWidgetUnchecked(
                        [](auto params, auto&& func, auto&& widget, auto&&... ts)
                        // -> AnyBuilder
                        {
                            auto oldParams = params;
                            return std::invoke(
                                    std::forward<decltype(widget)>(widget),
                                    std::invoke(
                                        std::forward<decltype(func)>(func),
                                        std::move(params),
                                        std::forward<decltype(ts)>(ts)...
                                        )
                                    )
                                    .setBuildParams(std::move(oldParams))
                                    ;

                        },
                        std::forward<decltype(func)>(func),
                        std::forward<decltype(widget)>(widget),
                        std::forward<decltype(ts)>(ts)...
                        );
            },
            std::forward<TFunc>(func),
            std::forward<Ts>(ts)...
            );
    }
} // namespace reactive::widget

