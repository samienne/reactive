#pragma once

#include <functional>

namespace btl
{
    namespace detail
    {
        template <typename TFunc, typename TTuple, size_t... S>
        auto tuple_map_seq(TFunc&& func, TTuple&& data,
                std::index_sequence<S...>)
            -> decltype(std::make_tuple(std::invoke(func, std::get<S>(
                                std::forward<TTuple>(data)))...))
        {
            return std::make_tuple(std::invoke(
                        func, std::get<S>(std::forward<TTuple>(data)))...);
        }
    }

    template <typename TFunc, typename TTuple>
    auto tuple_map(TTuple&& data, TFunc&& func)
    -> decltype(
            detail::tuple_map_seq(
                std::forward<TFunc>(func),
                std::forward<TTuple>(data),
                std::make_index_sequence<std::tuple_size<
                    std::decay_t<TTuple>>::value
                    >()
                )
            )
    {
        return detail::tuple_map_seq(std::forward<TFunc>(func),
                std::forward<TTuple>(data),
                std::make_index_sequence<std::tuple_size<
                    std::decay_t<TTuple>>::value
                    >());
    }

    template <typename TFunc>
    std::tuple<> tuple_map(std::tuple<>, TFunc&&)
    {
        return std::tuple<>();
    }
} // btl

