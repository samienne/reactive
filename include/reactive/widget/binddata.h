#pragma once

#include "widgettransform.h"

namespace reactive::widget
{
    template <typename T>
    auto bindData(T&& t)
    {
        return provideValues(std::forward<T>(t));
    }

} // namespace reactive::widget


