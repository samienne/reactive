#pragma once

#include "widgetmodifier.h"

namespace bqui::modifier
{
    template <typename T>
    auto setParamsObject(bq::signal::Signal<T, BuildParams> newParams)
    {
        return detail::makeWidgetModifierUnchecked(
            [](auto widget, auto&& newParams)
            {
                return widget::detail::makeWidgetUncheckedWithParams(
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
}

