#pragma once

#include "frameinfo.h"
#include "updateresult.h"
#include "signalresult.h"

#include "reactive/annotation.h"
#include "reactive/connection.h"

#include <btl/cloneoncopy.h>
#include <btl/all.h>
#include <btl/typetraits.h>

#include <functional>
#include <type_traits>
#include <chrono>
#include <optional>

namespace reactive::signal
{
    template <typename T>
    using decay_t = typename std::decay<T>::type;

    template <typename T>
    using evaluate_t = decltype(std::declval<decay_t<T> const&>().evaluate());

    template <typename T>
    using updateBegin_t = decltype(
            std::declval<decay_t<T>>().updateBegin(
                std::declval<signal::FrameInfo const>())
            );

    template <typename T>
    using updateEnd_t = decltype(
            std::declval<decay_t<T>>().updateEnd(
                std::declval<signal::FrameInfo const>())
            );

    template <typename T>
    using hasChanged_t = decltype(
            std::declval<const decay_t<T>&>().hasChanged()
            );

    template <typename T>
    using observe_t = decltype(
            std::declval<decay_t<T>>().observe(std::function<void()>())
            );

    template <typename T>
    using annotate_t = decltype(std::declval<const decay_t<T>&>().annotate());

    template <typename T, typename = void>
    struct CheckSignal : std::false_type {};

    /**
     * updateBegin can only call updateBegin.
     * updateBegin can be called multiple times without calling updateEnd.
     * Only updateEnd can be called after updateBegin.
     * Other functions can be called after updateEnd.
     * frame parameter in updateEnd must match previously called updateBegin.
     */
    template <typename T>
    struct CheckSignal<T, btl::void_t<
        evaluate_t<T>,
        updateBegin_t<T>,
        updateEnd_t<T>,
        hasChanged_t<T>,
        observe_t<T>
        >> : btl::All<
            std::is_same<bool, hasChanged_t<T>>,
            std::is_same<signal::UpdateResult, updateBegin_t<T>>,
            std::is_same<signal::UpdateResult, updateEnd_t<T>>,
            std::is_same<Connection, observe_t<T>>,
            std::is_same<Annotation, annotate_t<T>>,
            std::is_nothrow_move_constructible<std::decay_t<T>>,
            //std::is_nothrow_move_assignable<std::decay_t<T>>,
            btl::IsClonable<std::decay_t<T>>
        > {};

    template <typename T>
    /*struct IsSignal : std::conditional_t<
        std::is_reference<T>::value,
        IsSignal<std::decay_t<T>>,
        std::false_type> {};*/
    struct IsSignal : CheckSignal<T> {};

    template <typename T>
    struct IsSharedSignal :
        btl::All<
            IsSignal<T>,
            std::is_copy_constructible<T>
        >{};

    template <typename... Ts>
    struct AreSignals : btl::All<IsSignal<Ts>...> {};

    template <typename TSignal, typename = typename std::enable_if
        <
            IsSignal<TSignal>::value
        >::type>
    struct SignalValueType
    {
        using type = decay_t<decltype(std::declval<std::decay_t<TSignal>>().evaluate())>;
    };

    template <typename TSignal>
    using signal_value_t = typename SignalValueType<TSignal>::type;

    template <typename TSignal>
    using SignalType = decltype(std::declval<std::decay_t<TSignal>>().evaluate());

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
    using IsSignalType = detail::CheckSignalType<TSignal, SignalType<TSignal>, Ts...>;

    template <typename T, typename TSignature>
    using IsFunction = std::is_convertible<
            std::decay_t<T>,
            std::function<TSignature>
            >;
}

