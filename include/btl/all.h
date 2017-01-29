#pragma once

#include <utility>
#include <type_traits>

namespace btl
{
    template <typename...>
    struct All : std::true_type {};

    template <typename T>
    struct All<T> : T {};

    template <typename T,
             typename... Ts
        >
    struct All<T, Ts...> :
        std::conditional_t<
            T::value != false,
            All<Ts...>,
            T
        > {};

    inline bool all() noexcept
    {
        return true;
    }

    template <typename T, typename... Ts>
    bool all(T&& t, Ts&&... ts) noexcept
    {
        return std::forward<T>(t) && all(std::forward<Ts>(ts)...);
    }
} // btl

