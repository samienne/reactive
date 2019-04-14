#pragma once

#include "hidden.h"

#include <tuple>

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

        template <typename TFunc, typename U, typename TTuple, size_t... S>
        auto tuple_reduce_seq(TFunc&& f, U&& initial, TTuple&& data,
                std::index_sequence<S...>)
        {
            auto r = Reducer<TFunc, std::decay_t<U>>{
                std::forward<TFunc>(f),
                std::forward<U>(initial)
            };

            return std::move(
                    (std::move(r) << ...
                     << std::get<S>(std::forward<TTuple>(data))).value
                    );
        }
    }

    template <typename TFunc, typename T, typename TTuple>
    BTL_FORCE_INLINE auto tuple_reduce(T&& initial, TTuple&& tuple, TFunc&& func)
    -> decltype(
            detail::tuple_reduce_seq(
                std::forward<TFunc>(func),
                std::forward<T>(initial),
                std::forward<TTuple>(tuple),
                std::make_index_sequence<std::tuple_size<
                    std::decay_t<TTuple>>::value
                    >()
                )
            )
    {
        return detail::tuple_reduce_seq(
                std::forward<TFunc>(func),
                std::forward<T>(initial),
                std::forward<TTuple>(tuple),
                std::make_index_sequence<std::tuple_size<
                    std::decay_t<TTuple>>::value
                    >()
                );
    }
} // btl

