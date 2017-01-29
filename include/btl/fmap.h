#pragma once

#include "tuplemap.h"

#include "typetraits.h"

#include <vector>
#include <type_traits>

namespace btl
{
    namespace detail
    {
        struct Identity
        {
            template <typename T>
            auto operator()(T&& t) -> decltype(std::forward<T>(t))
            {
                return std::forward<T>(t);
            }
        };
    } // detail

    template <typename TFunc, typename T>
    auto fmap(std::vector<T> const& vec, TFunc&& func)
    {
        using ValueType = decltype(func(vec[0]));
        std::vector<btl::decay_t<ValueType>> result;
        result.reserve(vec.size());
        for (auto&& v : vec)
            result.push_back(std::forward<TFunc>(func)(v));
        return result;
    }

    template <typename TFunc, typename T>
    auto fmap(std::vector<T>& vec, TFunc&& func)
    {
        using ValueType = decltype(func(vec[0]));
        std::vector<btl::decay_t<ValueType>> result;
        result.reserve(vec.size());
        for (auto&& v : vec)
            result.push_back(std::forward<TFunc>(func)(v));
        return result;
    }

    template <typename TFunc, typename T>
    auto fmap(std::vector<T>&& vec, TFunc&& func)
    {
        using ValueType = decltype(func(vec[0]));
        std::vector<btl::decay_t<ValueType>> result;
        result.reserve(vec.size());
        for (auto&& v : std::move(vec))
            result.push_back(std::forward<TFunc>(func)(std::move(v)));
        return result;
    }

    template <typename TFunc, typename... Ts>
    auto fmap(std::tuple<Ts...> const& tuple, TFunc&& func)
    -> decltype(btl::tuple_map(tuple, std::forward<TFunc>(func)))
    {
        return btl::tuple_map(tuple, std::forward<TFunc>(func));
    }

    template <typename TFunc, typename... Ts>
    auto fmap(std::tuple<Ts...>& tuple, TFunc&& func)
    -> decltype(btl::tuple_map(tuple, std::forward<TFunc>(func)))
    {
        return btl::tuple_map(tuple, std::forward<TFunc>(func));
    }

    template <typename TFunc, typename... Ts>
    auto fmap(std::tuple<Ts...>&& tuple, TFunc&& func)
    -> decltype(btl::tuple_map(std::move(tuple), std::forward<TFunc>(func)))
    {
        return btl::tuple_map(std::move(tuple), std::forward<TFunc>(func));
    }

    template <typename T, typename = void>
    struct IsFunctor : std::false_type {};

    template <typename T>
    struct IsFunctor<T, void_t<decltype(
            btl::fmap(std::declval<T>(), detail::Identity())
            )>> : std::true_type {};

    static_assert(IsFunctor<std::vector<int>>::value, "");
    static_assert(IsFunctor<std::tuple<int, int>>::value, "");
    static_assert(!IsFunctor<int>::value, "");
} // btl

