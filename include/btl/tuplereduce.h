#pragma once

#include "apply.h"
#include "hidden.h"

//#include <utility>
//#include <type_traits>

namespace btl
{
    namespace detail
    {
        template <typename TFunc, typename T>
        struct Reducer
        {
            template <typename U>
            BTL_FORCE_INLINE auto operator<<(U&& u) &&
            {
                auto v = func(std::move(value), std::forward<U>(u));

                return Reducer<TFunc, std::decay_t<decltype(v)>>{
                        std::forward<TFunc>(func),
                        std::move(v)
                };
            }

            TFunc&& func;
            T value;
        };

        struct TupleReduce
        {
            template <typename TFunc, typename U, typename... Ts>
            BTL_FORCE_INLINE auto operator()(TFunc&& func, U&& initial, Ts&&... ts) const
            {
                auto r = Reducer<TFunc, std::decay_t<U>>{
                        std::forward<TFunc>(func),
                        std::forward<U>(initial)
                };

                return std::move(
                        (std::move(r) << ... << std::forward<Ts>(ts)).value
                        );
            }
        };
    }

    template <typename TFunc, typename T, typename TTuple>
    BTL_FORCE_INLINE auto tuple_reduce(T&& initial, TTuple&& tuple, TFunc&& func)
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

