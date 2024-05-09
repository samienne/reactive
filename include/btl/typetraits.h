#pragma once

#include "any.h"
#include "all.h"
#include "not.h"
#include "voidt.h"

#include <functional>
#include <type_traits>

namespace btl
{
    template <typename T>
    using decay_t = typename std::decay<T>::type;

    template <bool B, typename T, typename F>
    using conditional_t = typename std::conditional<B, T, F>::type;

    template< bool B, class T = void >
    using enable_if_t = typename std::enable_if<B,T>::type;

    template <typename T>
    using remove_reference_t = typename std::remove_reference<T>::type;

    template <typename TLhs, typename TRhs>
    using multiply_t = decltype(std::declval<TLhs const&>() *
            std::declval<TRhs const&>());

    template <typename T>
    using dereference_t = std::decay_t<decltype(*std::declval<T>())>;

    template <typename T, typename...>
    struct MakeType
    {
        using type = T;
    };

    template <typename... Ts>
    using make_type_t = typename MakeType<Ts...>::type;

    template <typename, typename = void>
    struct CanApply : std::false_type {};

    template <typename TFunc, typename... Ts>
    struct CanApply<TFunc(Ts...), void_t
    <
        decltype(std::declval<TFunc>()(std::declval<Ts>()...))
    >
    > :std::true_type {};

    template <typename TFunc, typename TSig>
    using IsFunction = std::is_convertible<TFunc, std::function<TSig>>;

    template <typename T, typename U, typename... Us>
    struct IsOneOf : Any<
        IsOneOf<T, std::remove_reference_t<U>>,
        IsOneOf<std::remove_reference_t<T>, std::remove_reference_t<Us>...>
    > {};

    template <typename T, typename U>
    struct IsOneOf<T, U> : std::is_same<
        btl::remove_reference_t<T>,
        btl::remove_reference_t<U>
    > {};

    template <typename T, typename U>
    using IsSimilar = std::is_same<std::decay_t<T>, std::decay_t<U>>;

    template <typename TFrom, typename TTo>
    using IsSafelyConvertable = All<
        std::is_convertible<TFrom, TTo>,
        Any<
            All<std::is_reference<TFrom>, std::is_reference<TTo>>,
            Not<std::is_reference<TTo>>
        >>;
    template <typename T>

    using CompareType = decltype(
            std::declval<std::decay_t<T>>() == std::declval<std::decay_t<T>>()
            );

    template <typename T, typename = void>
    struct IsEqualityComparable : std::false_type {};

    template <typename T>
    struct IsEqualityComparable<T, btl::void_t<
        CompareType<T>
        >> : std::true_type {};

}

