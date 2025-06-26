#pragma once

#include <functional>

namespace btl
{
    template <typename R, typename... TArgs>
    auto fun(R (*ptr)(TArgs...)) -> std::function<R(TArgs...)>
    {
        return std::function<R(TArgs...)>(ptr);
    }
}

