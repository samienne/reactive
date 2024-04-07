#pragma once

#include "shared.h"
#include "typed.h"
#include "updateresult.h"
#include "wrap.h"
#include "map.h"

#include <btl/future/future.h>
#include <btl/async.h>

#include <btl/connection.h>

namespace reactive::signal2
{
    template <typename TStorage, typename... Ts>
    class Signal;

    template <typename... Ts>
    using AnySignal = Signal<void, Ts...>;

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

        DataType initialize()
        {
            return sig_.initialize();
        }

        bool hasChanged(DataType const& data) const
        {
            return sig_.hasChanged(data);
        }

        auto evaluate(DataType const& data) const
        {
            return sig_.evaluate(data);
        }

        UpdateResult update(DataType& data, FrameInfo const& frame)
        {
            return sig_.update(data, frame);
        }

        template <typename TCallback>
        btl::connection observe(DataType& data, TCallback&& callback)
        {
            return sig_.observe(data, callback);
        }

        StorageType unwrap() &&
        {
            return std::move(sig_);
        }

        Signal<void, Ts...> eraseType() const
        {
            if constexpr (std::is_same_v<void, TStorage>)
            {
                return *this;
            }
            else
            {
                return wrap(SignalTypeless<Ts...>(makeTypedSignal(std::move(sig_))));
            }
        }

        Shared<StorageType, Ts...> share() const
        {
            return Shared<StorageType, Ts...>(sig_);
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

        template <typename... Us, typename = std::enable_if_t<
            btl::all(std::is_convertible_v<Ts, Us>...)
            >>
        operator AnySignal<Us...>() const
        {
            return AnySignal<Us...>(SignalTypeless<Us...>(
                        std::make_shared<SignalTyped<TStorage, Us...>>(sig_)
                        ));
        }

    private:
        StorageType sig_;
    };
} // namespace reactive::signal2

