#pragma once

#include <utility>

namespace btl
{
    struct Or
    {
        template <typename T, typename U>
        auto operator()(T&& t, U&& u) const
            -> decltype(std::forward<T>(t) || std::forward<U>(u))
        {
            return std::forward<T>(t) || std::forward<U>(u);
        }
    };
} // btl

