#pragma once

#include "apply.h"

#include <utility>
#include <type_traits>

namespace btl
{
    namespace detail
    {
        struct TupleReduce
        {
            template <typename TFunc, typename U, typename T, typename... Ts>
            auto operator()(TFunc&& func, U&& initial, T&& t, Ts&&... ts) const
                /*-> decltype(
                        (*this)(
                            std::forward<TFunc>(func),
                            func(std::forward<U>(initial), std::forward<T>(t)),
                            std::forward<Ts>(ts)...
                            )
                        )*/
            {
                return (*this)(
                        std::forward<TFunc>(func),
                        func(std::forward<U>(initial), std::forward<T>(t)),
                        std::forward<Ts>(ts)...
                        );
            }

            template <typename TFunc, typename U>
            auto operator()(TFunc&&, U&& initial) const
                -> std::decay_t<U>
            {
                return std::forward<U>(initial);
            }
        };
    }

    template <typename TFunc, typename T, typename TTuple>
    auto tuple_reduce(T&& initial, TTuple&& tuple, TFunc&& func)
        /*-> decltype(apply(detail::TupleReduce(),
                tuple_cat(
                    std::forward_as_tuple(
                        std::forward<TFunc>(func),
                        std::forward<T>(initial)
                        ),
                    std::forward<TTuple>(tuple)
                    )
                )
                )*/
    {
        auto tt = tuple_cat(
                    std::forward_as_tuple(
                        std::forward<TFunc>(func),
                        std::forward<T>(initial)
                        ),
                    std::forward<TTuple>(tuple)
                    );
        return apply(detail::TupleReduce(),
                std::move(tt)
                );
    }
} // btl

