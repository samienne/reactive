#pragma once

#include "widgetmap.h"

namespace reactive
{
    template <typename TFunc>
    auto bindWidgetMap(TFunc&& func)
    {
        return widgetValueConsumer(
            [func=std::forward<TFunc>(func)](auto widget, auto data) mutable
            {
                auto m = std::apply(func, std::move(data));

                return std::move(m)(std::move(widget));
            });
    }
} // namespace reactive

