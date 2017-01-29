#pragma once

#include "tuplelast.h"
#include "tupleallbutlast.h"

namespace btl
{
    template <typename TTuple>
    auto tuple_switch(TTuple&& tuple)
    -> decltype(
            std::tuple_cat(
                std::tuple<decltype(tuple_last(std::forward<TTuple>(tuple)))>(
                    tuple_last(std::forward<TTuple>(tuple))),
                tuple_all_but_last(std::forward<TTuple>(tuple))
                )
            )
    {
        return std::tuple_cat(
                std::tuple<decltype(tuple_last(std::forward<TTuple>(tuple)))>(
                    tuple_last(std::forward<TTuple>(tuple))),
                tuple_all_but_last(std::forward<TTuple>(tuple))
                );
    }
} // btl

