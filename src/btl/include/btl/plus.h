#pragma once

#include <utility>

namespace btl
{
    struct Plus
    {
        template <typename T, typename U>
        auto operator()(T&& t, U&& u)
            -> typename std::decay<decltype(
                    std::forward<T>(t)+std::forward<U>(u))>::type
        {
            return std::forward<T>(t) + std::forward<U>(u);
        }
    };
} // btl

