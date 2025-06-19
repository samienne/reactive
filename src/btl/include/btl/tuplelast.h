#pragma once

#include <tuple>

namespace btl
{
    template <typename T, typename... Ts>
    auto tuple_last(std::tuple<T, Ts...>&& tuple)
    -> std::tuple_element_t<sizeof...(Ts), std::decay_t<decltype(tuple)>>
    //-> decltype(std::get<sizeof...(Ts)>(std::move(tuple)))
    {
        return std::get<sizeof...(Ts)>(std::move(tuple));
    }

    template <typename T, typename... Ts>
    auto tuple_last(std::tuple<T, Ts...>& tuple)
    -> std::tuple_element_t<sizeof...(Ts), std::decay_t<decltype(tuple)>>
    //-> decltype(std::get<sizeof...(Ts)>(tuple))
    {
        return std::get<sizeof...(Ts)>(tuple);
    }

    template <typename T, typename... Ts>
    auto tuple_last(std::tuple<T, Ts...> const& tuple)
    -> std::tuple_element_t<sizeof...(Ts), std::decay_t<decltype(tuple)>>
    //-> decltype(std::get<sizeof...(Ts)>(tuple))
    {
        return std::get<sizeof...(Ts)>(tuple);
    }
} // btl


