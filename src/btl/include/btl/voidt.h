#pragma once

namespace btl
{
    template <typename...>
    struct void_
    {
        using type = void;
    };

    template <typename... T>
    using void_t = typename void_<T...>::type;

    template <typename... T>
    using VoidType = typename void_<T...>::type;
} // btl

