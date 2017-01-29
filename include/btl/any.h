#pragma once

#include <type_traits>

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
} // btl

