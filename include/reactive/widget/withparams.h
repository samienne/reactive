#pragma once

#include "withparamsobject.h"
#include "widget.h"

#include "reactive/signal/sharedsignal.h"

#include <type_traits>

namespace reactive::widget
{
    template <typename TTag>
    auto tagValueOrDefault(BuildParams const& params)
    {
        auto param = params.get<TTag>();
        if (param)
            return *param;

        return TTag::defaultValue;
    }

    template <typename... TTags, typename TFunc, typename... Ts, typename =
        std::enable_if_t<
            std::is_invocable_r_v<AnyWidget, TFunc, AnyWidget,
                AnySharedSignal<typename TTags::type>..., Ts&&...>
        >>
    auto withParams(TFunc&& func, Ts&&... ts)
    {
        return withParamsObject([]
            (auto widget, BuildParams const& params, auto&& func, auto&&... ts)
            {
                return std::invoke(
                        std::forward<decltype(func)>(func),
                        std::move(widget),
                        tagValueOrDefault<TTags>(params)...,
                        std::forward<decltype(ts)>(ts)...
                        );
            },
            std::forward<TFunc>(func),
            std::forward<Ts>(ts)...
            );
    }
} // namespace reactive::widget
