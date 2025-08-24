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
        return makeWidgetModifier(
            [](auto widget, BuildParams oldParams, auto func, auto&&... ts)
            {
                auto newParams = std::invoke(func, oldParams,
                        std::forward<decltype(ts)>(ts)...);

                auto builder = std::move(widget)(newParams)
                    .setBuildParams(newParams)
                    ;

                return makeWidgetFromBuilder(std::move(builder));
            },
            provider::provideBuildParams(),
            std::forward<TFunc>(func),
            std::forward<Ts>(ts)...
            );
    }
}

