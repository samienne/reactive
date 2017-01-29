#pragma once

#include <tuple>

namespace btl
{
    namespace detail
    {
        template <typename TTuple, size_t... I>
        auto tuple_reverse_impl(TTuple&& tuple, std::index_sequence<I...>)
        {
            return std::tuple<
                std::tuple_element_t<sizeof...(I) - 1 - I, TTuple>...
                >(std::get<sizeof...(I) - 1 - I>(
                            std::forward<TTuple>(tuple))...);
        }

    } // detail

    template <typename TTuple>
    auto tuple_reverse(TTuple&& tuple)
    -> decltype(
            detail::tuple_reverse_impl(std::forward<TTuple>(tuple),
                std::make_index_sequence<std::tuple_size<TTuple>::value>())
            )
    {
        return detail::tuple_reverse_impl(std::forward<TTuple>(tuple),
                std::make_index_sequence<std::tuple_size<TTuple>::value>());
    }

    inline std::tuple<> tuple_reverse(std::tuple<> const&)
    {
        return {};
    }


} // btl

