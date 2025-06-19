#pragma once

#include "tuplerawget.h"

#include <tuple>

namespace btl
{
    namespace detail
    {
        template <typename TTuple, size_t... I>
        auto tuple_all_but_last_impl(TTuple&& tuple, std::index_sequence<I...>)
        {
            return std::tuple<
                std::tuple_element_t<I, std::decay_t<TTuple>>...
                >(tuple_raw_get<I>(std::forward<TTuple>(tuple))...);
        }
    } // detail

    template <typename TTuple, typename = std::enable_if_t<
        (std::tuple_size<std::decay_t<TTuple>>::value >= 1)>>
    auto tuple_all_but_last(TTuple&& tuple)
    -> decltype(
            detail::tuple_all_but_last_impl(
                std::forward<TTuple>(tuple),
                    std::make_index_sequence<
                    std::tuple_size<std::decay_t<TTuple>>::value - 1
                    >())
            )
    {
        return detail::tuple_all_but_last_impl(
                std::forward<TTuple>(tuple),
                std::make_index_sequence<
                std::tuple_size<std::decay_t<TTuple>>::value - 1
                >());
    }

} // btl

