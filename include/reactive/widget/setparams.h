#pragma once

#include "modifyparamsobject.h"
#include "widget.h"

#include <initializer_list>

namespace reactive::widget
{
    template <typename... TTags, typename... Ts>
    auto setParams(signal2::Signal<Ts, typename TTags::type>... values)
    {
        if constexpr (sizeof...(TTags) == 0)
        {
            return detail::makeWidgetModifierUnchecked([](auto widget)
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

    /*
    template <typename... TTags, typename... Ts>
    auto setParams(signal2::Signal<Ts, typename TTags::type>... values)
    {
        return setParams<TTags...>(share(std::move(values)...));
    }
    */

    template <typename... TTags>
    auto setParams(typename TTags::type... values)
    {
        return setParams<TTags...>(signal2::constant(std::move(values)...));
    }
} // namespace reactive::widget

