#pragma once

#include "frameinfo.h"
#include "updateresult.h"
#include "signalresult.h"

#include "reactive/connection.h"

#include <btl/cloneoncopy.h>
#include <btl/all.h>
#include <btl/typetraits.h>

#include <functional>
#include <type_traits>

namespace reactive::signal2
{
    template <typename T>
    using initialize_t = decltype(std::declval<std::decay_t<T> const&>().initialize());

    template <typename T>
    using evaluate_t = decltype(std::declval<std::decay_t<T> const&>().evaluate());

    template <typename T>
    using update_t = decltype(
            std::declval<std::decay_t<T>>().update(
                std::declval<std::decay_t<initialize_t<T>>>(),
                std::declval<FrameInfo const>())
            );

    template <typename T>
    using hasChanged_t = decltype(
            std::declval<const std::decay_t<T>&>().hasChanged(
                std::declval<std::decay_t<initialize_t<T> const&>>()
                )
            );

    template <typename T>
    using observe_t = decltype(
            std::declval<std::decay_t<T>>().observe(std::function<void()>())
            );

    template <typename T, typename = void>
    struct CheckSignal : std::false_type {};

    template <typename T>
    struct CheckSignal<T, btl::void_t<
        initialize_t<T>,
        evaluate_t<T>,
        update_t<T>,
        hasChanged_t<T>,
        observe_t<T>
        >> : btl::All<
            std::is_same<bool, hasChanged_t<T>>,
            std::is_same<UpdateResult, update_t<T>>,
            std::is_same<Connection, observe_t<T>>,
            std::is_nothrow_move_constructible<std::decay_t<T>>,
            btl::IsClonable<std::decay_t<T>>
        > {};

    template <typename T>
    struct IsSignal : CheckSignal<T> {};

    template <typename TSignal>
    struct SignalDataType : std::decay<decltype(std::declval<TSignal>()
            .initialize())>
    {
    };

    template <typename T, typename... Ts>
    class Signal;

    template <typename... Ts>
    class AnySignal;

    template <typename T, typename... Ts>
    struct SignalDataType<Signal<T, Ts...>>
    {
        using type = std::decay_t<
            decltype(std::declval<Signal<T, Ts...>>().unwrap().initialize())
            >;
    };

    template <typename... Ts>
    struct SignalDataType<AnySignal<Ts...>> : SignalDataType<Signal<void, Ts...>>
    {
    };

    template <typename TSignal>
    using SignalDataTypeT = typename SignalDataType<TSignal>::type;

    template <typename TSignal>
    struct SignalValueType
    {
        using type = std::decay_t<decltype(std::declval<std::decay_t<TSignal>>()
                .evaluate(std::declval<SignalDataTypeT<TSignal>>))>;
    };

    template <typename TSignal>
    using signal_value_t = typename SignalValueType<TSignal>::type;

    template <typename TSignal>
    using SignalTypeT = decltype(std::declval<std::decay_t<TSignal> const&>()
            .evaluate(std::declval<SignalDataTypeT<TSignal> const&>())
            );

    namespace detail
    {
        template <typename TSignal, typename TRet, typename... Ts>
        struct CheckSignalType : std::false_type {};

        template <typename TSignal, typename TRet, typename T>
        struct CheckSignalType<TSignal, TRet, T> : std::is_convertible<TRet, T> {};

        template <typename TSignal, typename... Ts, typename... Us>
        struct CheckSignalType<TSignal, SignalResult<Us...>, Ts...> :
            btl::All<std::is_convertible<Us, Ts>...> {};
    } // namespace detail


    template <typename TSignal, typename... Ts>
    using IsSignalType = detail::CheckSignalType<TSignal, SignalTypeT<TSignal>, Ts...>;

    template <typename T, typename TSignature>
    using IsFunction = std::is_convertible<
            std::decay_t<T>,
            std::function<TSignature>
            >;

    template <typename T>
    struct DecaySignalResult
    {
    };

    template <typename... Ts>
    struct DecaySignalResult<SignalResult<Ts...>>
    {
        using type = SignalResult<std::decay_t<Ts>...>;
    };

    template <typename T>
    using DecaySignalResultT = typename DecaySignalResult<T>::type;

}

