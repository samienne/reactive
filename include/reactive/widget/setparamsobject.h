#pragma once

#include "widget.h"

namespace reactive::widget
{
    template <typename T>
    auto setParamsObject(Signal<T, BuildParams> newParams)
    {
        return detail::makeWidgetModifierUnchecked(
            [](auto widget, auto&& newParams)
            {
                return detail::makeWidgetUncheckedWithParams(
                        [](BuildParams params, auto&& widget, auto&& newParams)
                        // -> AnyBuilder
                        {
                            return std::invoke(
                                    std::forward<decltype(widget)>(widget),
                                    std::forward<decltype(newParams)>(newParams)
                                    )
                                    .setBuildParams(std::move(params))
                                    ;

                        },
                        std::move(widget),
                        std::forward<decltype(newParams)>(newParams)
                        );
            },
            std::move(newParams)
            );
    }
} // namespace reactive::widget

