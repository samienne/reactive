#pragma once

#include "widgetmodifier.h"

namespace bqui::modifier
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
                return widget::detail::makeWidgetUncheckedWithParams(
                        [](auto params, auto&& func, auto&& widget, auto&&... ts)
                        // -> AnyBuilder
                        {
                            auto newParams = std::invoke(
                                        std::forward<decltype(func)>(func),
                                        params,
                                        std::forward<decltype(ts)>(ts)...
                                        );

                            return std::invoke(
                                    std::forward<decltype(widget)>(widget),
                                    newParams
                                    )
                                    .setBuildParams(params)
                                    .preMap([newParams](auto element)
                                        {
                                            return std::move(element)
                                                .setParams(newParams)
                                                ;
                                        })
                                    .map([params](auto element)
                                        {
                                            return std::move(element)
                                                .setParams(params)
                                                ;
                                        })
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
}

