#pragma once

#include <tuple>

namespace btl
{
    namespace detail
    {
        template <typename TTuple, size_t... I>
        auto tuple_tail_impl(TTuple&& tuple, std::index_sequence<I...>)
        {
            return std::tuple<
                std::tuple_element_t<I + 1, std::decay_t<TTuple>>...
                >(std::get<I + 1>(std::forward<TTuple>(tuple))...);
        }
    } // detail

    template <typename TTuple, typename = std::enable_if_t<
        (std::tuple_size<std::decay_t<TTuple>>::value >= 1)>>
    auto tuple_tail(TTuple&& tuple)
    -> decltype(
            detail::tuple_tail_impl(
                std::forward<TTuple>(tuple),
                    std::make_index_sequence<
                    std::tuple_size<TTuple>::value - 1
                    >())
            )
    {
        return detail::tuple_tail_impl(
                std::forward<TTuple>(tuple),
                std::make_index_sequence<
                std::tuple_size<TTuple>::value - 1
                >());
    }

} // btl

