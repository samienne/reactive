#pragma once

#include "tupleforeach.h"

#include <vector>
#include <tuple>

namespace btl
{
    namespace detail
    {
        struct NoOp
        {
            template <typename T>
            void operator()(T&&) {}
        };
    } // detail

    template <typename TFunc, typename T>
    void forEach(std::vector<T> const& vec, TFunc&& func)
    {
        for (auto const& v : vec)
            func(v);
    }

    template <typename TFunc, typename T>
    void forEach(std::vector<T>& vec, TFunc&& func)
    {
        for (auto& v : vec)
            func(v);
    }

    template <typename TFunc, typename T>
    void forEach(std::vector<T>&& vec, TFunc&& func)
    {
        for (auto&& v : vec)
            func(std::move(v));
    }

    template <typename TFunc, typename... Ts>
    void forEach(std::tuple<Ts...>& tuple, TFunc&& func)
    {
        return tuple_foreach(tuple, std::forward<TFunc>(func));
    }

    template <typename TFunc, typename... Ts>
    void forEach(std::tuple<Ts...> const& tuple, TFunc&& func)
    {
        return tuple_foreach(tuple, std::forward<TFunc>(func));
    }

    template <typename TFunc, typename... Ts>
    void forEach(std::tuple<Ts...>&& tuple, TFunc&& func)
    {
        return tuple_foreach(std::move(tuple), std::forward<TFunc>(func));
    }

    template <typename T, typename = void>
    struct IsForEachable : std::false_type {};

    template <typename T>
    struct IsForEachable<T, void_t<
        decltype(btl::forEach(std::declval<T>(), detail::NoOp()))
        >
    >: std::true_type {};

    static_assert(IsForEachable<std::vector<int>>::value, "");
    static_assert(IsForEachable<std::tuple<int, int>>::value, "");
} // btl

