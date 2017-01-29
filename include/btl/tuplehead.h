#pragma once

#include <tuple>

namespace btl
{
    template <typename T, typename... Ts>
    auto tuple_head(std::tuple<T, Ts...>&& tuple)
    -> decltype(std::get<0>(std::move(tuple)))
    {
        return std::get<0>(std::move(tuple));
    }

    template <typename T, typename... Ts>
    auto tuple_head(std::tuple<T, Ts...>& tuple)
    -> decltype(std::get<0>(tuple))
    {
        return std::get<0>(tuple);
    }

    template <typename T, typename... Ts>
    auto tuple_head(std::tuple<T, Ts...> const& tuple)
    -> decltype(std::get<0>(tuple))
    {
        return std::get<0>(tuple);
    }
} // btl

