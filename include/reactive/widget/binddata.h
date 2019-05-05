#pragma once

#include "reactive/widgetvalueprovider.h"
#include <btl/pushback.h>

namespace reactive::widget
{
    template <typename T>
    auto bindData(T&& t)
    {
        return widgetValueProvider([t=std::forward<T>(t)](auto widget, auto data)
        {
            return std::make_pair(
                    std::move(widget),
                    btl::pushBack(std::move(data), std::move(t))
                    );
        });
    }

} // namespace reactive::widget


