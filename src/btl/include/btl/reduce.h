#pragma once

#include "tuplereduce.h"

#include "typetraits.h"

#include <type_traits>
#include <vector>

namespace btl
{
    namespace detail
    {
        struct Replace
        {
            template <typename T, typename U>
            btl::decay_t<U> operator()(T&&, U&& u) const
            {
                return std::forward<U>(u);
            }
        };
    } // detail

    // Vector
    template <typename T, typename TInitial, typename TFunc>
    auto reduce(TInitial&& initial, std::vector<T> const& vec,
            TFunc&& f) noexcept
    {
        auto r = std::forward<TInitial>(initial);
        for (auto&& v : vec)
            r = f(std::move(r), v);
        return r;
    }

    template <typename T, typename TInitial, typename TFunc>
    auto reduce(TInitial&& initial, std::vector<T>& vec,
            TFunc&& f) noexcept
    {
        auto r = std::forward<TInitial>(initial);
        for (auto&& v : vec)
            r = f(std::move(r), v);
        return r;
    }

    template <typename T, typename TInitial, typename TFunc>
    auto reduce(TInitial&& initial, std::vector<T>&& vec,
            TFunc&& f) noexcept
    {
        auto r = std::forward<TInitial>(initial);
        for (auto&& v : vec)
            r = f(std::move(r), std::move(v));
        return r;
    }

    // Tuple
    template <typename TFunc, typename TInitial, typename... Ts>
    auto reduce(TInitial&& initial, std::tuple<Ts...> const& tuple,
            TFunc&& func)
    {
        return btl::tuple_reduce(std::forward<TInitial>(initial),
                tuple, std::forward<TFunc>(func));
    }

    template <typename TFunc, typename TInitial, typename... Ts>
    auto reduce(TInitial&& initial, std::tuple<Ts...>& tuple,
            TFunc&& func)
    {
        return btl::tuple_reduce(std::forward<TInitial>(initial),
                tuple, std::forward<TFunc>(func));
    }

    template <typename TFunc, typename TInitial, typename... Ts>
    auto reduce(TInitial&& initial, std::tuple<Ts...>&& tuple,
            TFunc&& func)
    {
        return btl::tuple_reduce(std::forward<TInitial>(initial),
                std::move(tuple), std::forward<TFunc>(func));
    }

    template <typename T, typename = void>
    struct IsReducable : std::false_type {};

    template <typename T>
    struct IsReducable<T, void_t<decltype(
                btl::reduce(0, std::declval<T>(), detail::Replace())
            )>> : std::true_type {};

} // btl

