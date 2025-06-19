#pragma once

#include <type_traits>
#include <functional>
#include <string>

namespace btl
{
    template <typename... Ts>
    struct ParamPack
    {
    };

    template <template <typename...> typename TTemplate, typename T>
    struct ParamPackApply
    {
    };

    template <template <typename...> typename TTemplate, typename... Ts>
    struct ParamPackApply<TTemplate, ParamPack<Ts...>>
    {
        using type = TTemplate<Ts...>;
    };

    template <template <typename...> typename TTemplate, typename T>
    using ParamPackApplyT = typename ParamPackApply<TTemplate, T>::type;

    template <template <typename> typename TMap, typename TPack>
    struct ParamPackMap
    {
    };


    template <template <typename> typename TMap, typename... Ts>
    struct ParamPackMap<TMap, ParamPack<Ts...>>
    {
        using type = ParamPack<TMap<Ts>...>;
    };

    template <template <typename> typename TMap, typename TPack>
    using ParamPackMapT = typename ParamPackMap<TMap, TPack>::type;

    template <typename T>
    struct UnpackTuple
    {
    };

    template <typename... Ts>
    struct UnpackTuple<std::tuple<Ts...>>
    {
        using type = ParamPack<Ts...>;
    };

    template <typename T>
    using UnpackTupleT = typename UnpackTuple<T>::type;

    template <typename... Ts>
    struct ConcatParamPacks
    {
    };

    template <>
    struct ConcatParamPacks<>
    {
        using type = ParamPack<>;
    };

    template <typename... Ts>
    struct ConcatParamPacks<ParamPack<Ts...>>
    {
        using type = ParamPack<Ts...>;
    };

    template <typename... Ts, typename... Us, typename... Vs>
    struct ConcatParamPacks<ParamPack<Ts...>, ParamPack<Us...>, Vs...> :
    ConcatParamPacks<ParamPack<Ts..., Us...>, Vs...>
    {
    };

    template <typename... Ts>
    using ConcatParamPacksT = typename ConcatParamPacks<Ts...>::type;

    template <typename T>
    struct ToParamPack
    {
        using type = ParamPack<T>;
    };

    template <typename... Ts>
    struct ToParamPack<ParamPack<Ts...>>
    {
        using type = ParamPack<Ts...>;
    };

    template <typename T>
    using ToParamPackT = typename ToParamPack<T>::type;

    template <typename R, typename F, typename TParamPack>
    struct IsInvocableRWithParamPack : std::false_type
    {
    };

    template <typename R, typename F, typename... Ts>
    struct IsInvocableRWithParamPack<R, F, ParamPack<Ts...>> :
    std::is_invocable_r<R, F, Ts...>
    {
    };

    template <typename R, typename F, typename... Args>
    struct IsInvocableR : IsInvocableRWithParamPack<R, F,
        ConcatParamPacksT<ToParamPackT<Args>...>
        >
    {
    };

    template <typename R, typename F, typename... Args>
    inline constexpr bool isInvocableRV = IsInvocableR<R, F, Args...>::value;
} // namespace btl

