#pragma once

#include <tuple>
#include <functional>

namespace btl
{
    namespace detail
    {
        template <typename TFunc, typename TTuple, size_t... S>
        auto apply_seq(TFunc&& f, TTuple&& data, std::index_sequence<S...>)
        -> decltype(
                std::invoke(
                    std::forward<TFunc>(f),
                    std::get<S>(std::forward<TTuple>(data))...
                    )
                )
        {
            return std::invoke(
                    std::forward<TFunc>(f),
                    std::get<S>(std::forward<TTuple>(data))...
                    );
        }
    }

    template <typename TFunc, typename TTuple>
    auto apply(TFunc&& func, TTuple&& data)
        -> decltype(
                detail::apply_seq(
                    std::forward<TFunc>(func),
                    std::forward<TTuple>(data),
                    std::make_index_sequence<std::tuple_size<
                        std::decay_t<TTuple>>::value
                        >())
                )
    {
        return detail::apply_seq(
                std::forward<TFunc>(func),
                std::forward<TTuple>(data),
                std::make_index_sequence<std::tuple_size<
                    std::decay_t<TTuple>>::value>()
                );
    }

    template <typename TFunc>
    auto apply(TFunc&& func, std::tuple<> const&)
        -> decltype(std::forward<TFunc>(func)())
    {
        return std::forward<TFunc>(func)();
    }

    template <typename TFunc>
    auto apply(TFunc&& func, std::tuple<>&&)
        -> decltype(std::forward<TFunc>(func)())
    {
        return std::forward<TFunc>(func)();
    }
}

