#pragma once

#include "join.h"
#include "shared.h"
#include "typed.h"
#include "updateresult.h"
#include "wrap.h"
#include "map.h"
#include "conditional.h"

#include <btl/future/future.h>
#include <btl/async.h>

#include <btl/connection.h>

namespace reactive::signal2
{
    template <typename TStorage, typename... Ts>
    class Signal;

    template <typename... Ts>
    class AnySignal;

    template <typename TStorage, typename... Ts>
    class Signal
    {
    public:
        using StorageType = std::conditional_t<
            std::is_same_v<void, TStorage>,
            SignalTypeless<Ts...>,
            TStorage
            >;

        using DataType = typename StorageType::DataType;

        Signal(Signal const&) = default;
        Signal(Signal&&) noexcept = default;

        Signal& operator=(Signal const&) = default;
        Signal& operator=(Signal&&) noexcept = default;

        Signal(StorageType sig) :
            sig_(std::move(sig))
        {
        }

        StorageType& unwrap() &
        {
            return sig_;
        }

        StorageType unwrap() &&
        {
            return std::move(sig_);
        }

        StorageType const& unwrap() const&
        {
            return sig_;
        }

        AnySignal<Ts...> eraseType() const
        {
            if constexpr (std::is_same_v<void, TStorage>)
            {
                return *this;
            }
            else
            {
                return wrap(makeTypelessSignal(*this));
            }
        }

        Signal<Shared<StorageType, Ts...>, Ts...> share() const
        {
            return wrap(Shared<StorageType, Ts...>(sig_));
        }

        auto join() const
        {
            return wrap(Join<TStorage>(sig_));
        }

        Signal clone() const
        {
            return *this;
        }

        template <typename TFunc>
        auto map(TFunc&& func) const&
        {
            return clone().map(std::forward<TFunc>(func));
        }

        template <typename TFunc>
        auto map(TFunc&& func) &&
        {
            return wrap(Map<std::decay_t<TFunc>, StorageType>(
                        std::forward<TFunc>(func),
                        std::move(sig_)));
        }

        template <typename T, typename U, typename... Us>
        auto conditional(Signal<T, Us...> trueSignal, Signal<U, Us...> falseSignal) const
            -> decltype(signal2::conditional(*this, std::move(trueSignal),
                        std::move(falseSignal)))
        {
            return signal2::conditional(*this, std::move(trueSignal),
                    std::move(falseSignal));
        }

    protected:
        StorageType sig_;
    };

    template <typename... Ts>
    class AnySignal : public Signal<void, Ts...>
    {
    public:
        template <typename TStorage, typename... Us, typename = std::enable_if_t<
            btl::all(std::is_convertible_v<Us, Ts>...)
            >>
        AnySignal(Signal<TStorage, Us...> const& rhs) :
            Signal<void, Ts...>(makeTypelessSignal<Ts...>(rhs))
        {
        }

        template <typename TStorage, typename... Us, typename = std::enable_if_t<
            btl::all(std::is_convertible_v<Us, Ts>...)
            >>
        AnySignal(Signal<TStorage, Us...>&& rhs) noexcept:
            Signal<void, Ts...>(makeTypelessSignal<Ts...>(std::move(rhs)))
        {
        }

        template <typename TStorage, typename... Us, typename = std::enable_if_t<
            btl::all(std::is_convertible_v<Us, Ts>...)
            >>
        AnySignal operator=(Signal<TStorage, Us...> const& rhs)
        {
            Signal<void, Ts...>::sig_ = makeTypelessSignal<Ts...>(rhs);
            return *this;
        }

        template <typename TStorage, typename... Us, typename = std::enable_if_t<
            btl::all(std::is_convertible_v<Us, Ts>...)
            >>
        AnySignal operator=(Signal<TStorage, Us...>&& rhs) noexcept
        {
            Signal<void, Ts...>::sig_ = makeTypelessSignal<Ts...>(rhs);
            return *this;
        }
    };

    template <typename... Ts>
    struct IsSignal<Signal<Ts...>> : std::true_type {};
} // namespace reactive::signal2

