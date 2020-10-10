#pragma once

#include <tuple>

#include <cstddef>

namespace btl
{
    template <size_t I, typename TTuple>
    auto tuple_raw_get(TTuple&& tuple)
    -> std::tuple_element_t<I, std::decay_t<TTuple>>
    {
        return static_cast<
            std::tuple_element_t<I, std::decay_t<TTuple>>
            >
            (std::get<I>(std::forward<TTuple>(tuple)));
    }
} // btl

