#pragma once

#include <tuple>

namespace btl
{
    template <typename TTuple, typename T>
    auto pushBack(TTuple&& tuple, T&& t)
    -> decltype(std::tuple_cat(
                std::forward<TTuple>(tuple),
                std::make_tuple(std::forward<T>(t))
                ))
    {
        return std::tuple_cat(
                std::forward<TTuple>(tuple),
                std::make_tuple(std::forward<T>(t))
                );
    }

    template <typename TTuple, typename T>
    auto pushFront(TTuple&& tuple, T&& t)
    -> decltype(
            std::tuple_cat(
                std::make_tuple(std::forward<T>(t)),
                std::forward<TTuple>(tuple)
                )
            )
    {
        return std::tuple_cat(
                std::make_tuple(std::forward<T>(t)),
                std::forward<TTuple>(tuple)
                );
    }
} // btl

