#pragma once

#include <type_traits>
#include <utility>

namespace btl
{
    template <typename... Ts>
    struct Any : std::false_type {};

    template <typename T>
    struct Any<T> : T {};

    template <typename T, typename... Ts>
    struct Any<T, Ts...> :
        std::conditional_t<
            T::value != false,
            T,
            Any<Ts...>
        > {};

    constexpr inline bool any() noexcept
    {
        return false;
    }

    template <typename T, typename... Ts>
    constexpr bool any(T&& t, Ts&&... ts) noexcept
    {
        return std::forward<T>(t) || any(std::forward<Ts>(ts)...);
    }
} // btl

