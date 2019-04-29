#pragma once

#include "reactive/widgetvalueprovider.h"

namespace reactive::widget
{
    template <typename T>
    auto bindData(T&& t)
    {
        return widgetValueProvider([t=std::forward<T>(t)](auto widget)
        {
            return std::move(widget)
                .addData(std::move(t))
                ;
        });
    }

} // namespace reactive::widget


