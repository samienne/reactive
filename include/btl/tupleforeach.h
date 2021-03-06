#pragma once

#include "typetraits.h"

#include <functional>

namespace btl
{
    namespace detail
    {
        template <typename TFunc, typename TTuple, size_t... S>
        void tuple_foreach_seq(TFunc&& f, TTuple&& data,
                std::index_sequence<S...>)
        {
            using sink = int[];
            (void)sink{1,
                (std::invoke(f, std::get<S>(std::forward<TTuple>(data))),
                 void(), int{})...
            };
        }

    }

    template <typename TFunc, typename TTuple>
    auto tuple_foreach(TTuple&& data, TFunc&& func)
    -> decltype(
            detail::tuple_foreach_seq(
                std::forward<TFunc>(func),
                std::forward<TTuple>(data),
                std::make_index_sequence<
                    std::tuple_size<
                        std::decay_t<TTuple>
                        >::value
                    >()
                )
            )
    {
        detail::tuple_foreach_seq(
                std::forward<TFunc>(func),
                std::forward<TTuple>(data),
                std::make_index_sequence<
                    std::tuple_size<
                        std::decay_t<TTuple>
                        >::value
                    >()
                );
    }
} // btl

