#pragma once

#include "widgetmap.h"

namespace reactive
{
    template <typename TFunc>
    auto bindWidgetMap(TFunc&& func)
    {
        return forceMapWidget([func=std::forward<TFunc>(func)](auto widget) mutable
        {
            auto m = std::apply(func, std::move(widget.getData()));

            return std::move(m)(std::move(widget).dropData());
        });
    }

} // namespace reactive

