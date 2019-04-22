#pragma once

#include "reactive/widgetmap.h"

namespace reactive::widget
{
    template <typename T>
    auto bindData(T&& t)
    {
        return forceMapWidget([t=std::forward<T>(t)](auto widget)
        {
            return std::move(widget)
                .addData(std::move(t))
                ;
        });
    }

} // namespace reactive::widget


