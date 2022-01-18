#pragma once

#include "modifyparamsobject.h"

#include <initializer_list>

namespace reactive::widget
{
    template <typename... TTags>
    auto setParams(signal::AnySharedSignal<typename TTags::type>... values)
    {
        if constexpr (sizeof...(TTags) == 0)
        {
            return makeWidgetModifier([](auto widget)
                {
                    return widget;
                });
        }
        else
        {
            return modifyParamsObject([](BuildParams params, auto&&... ts)
                {
                    (void)std::initializer_list<int>{
                        ((void)params.set<TTags>(std::forward<decltype(ts)>(ts)), 0)...
                        };

                    return params;
                },
                std::move(values)...
                );
        }
    }
} // namespace reactive::widget

