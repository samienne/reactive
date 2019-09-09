#pragma once

#include "reactive/widgetvaluemapper.h"

#include <btl/cloneoncopy.h>

#include <tuple>

namespace reactive::widget
{
    template <typename TFunc>
    auto mapWidgetData(TFunc&& func)
    {
        return widgetValueMapper([func=std::forward<TFunc>(func)]
        (auto widget, auto data)
        -> std::decay_t<decltype(
            std::make_pair(
                std::move(widget),
                btl::cloneOnCopy(std::apply(func, std::move(data)))
                )
            )>
        {
            return std::make_pair(
                    std::move(widget),
                    btl::cloneOnCopy(std::apply(func, std::move(data)))
                    );
        });
    }
} // namespace reactive::widget

