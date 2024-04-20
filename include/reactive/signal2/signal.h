#pragma once

#include "input.h"
#include "join.h"
#include "shared.h"
#include "typed.h"
#include "wrap.h"
#include "map.h"
#include "conditional.h"
#include "weak.h"

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
    class SignalWithStorage
    {
    public:
        SignalWithStorage(TStorage sig) :
            sig_(std::move(sig))
        {
        }

    protected:
        TStorage sig_;
    };

    template <typename... Ts>
    class SignalWithStorage<void, Ts...>
    {
    public:
        SignalWithStorage(SignalTypeless<Ts...> sig) :
            sig_(std::move(sig))
        {
        }

    protected:
        SignalTypeless<Ts...> sig_;
    };


    template <typename TStorage, typename... Ts>
    class SignalWithWeak : public SignalWithStorage<TStorage, Ts...>
    {
    public:
        using Super = SignalWithStorage<TStorage, Ts...>;
        using Super::Super;
    };

    template <typename T, typename... Ts>
    class SignalWithWeak<Shared<T, Ts...>, Ts...> : public SignalWithStorage<Shared<T, Ts...>, Ts...>
    {
    public:
        using Super = SignalWithStorage<Shared<T, Ts...>, Ts...>;
        using Super::Super;

        Signal<Weak<Ts...>, Ts...> weak() const
        {
            return Super::sig_.weak();
        }
    };

    template <typename T, typename... Ts>
    class SignalWithConditional : public SignalWithWeak<T, Ts...>
    {
    public:
        using Super = SignalWithWeak<T, Ts...>;
        using Super::Super;
    };

    template <typename T>
    class SignalWithConditional<T, bool> : public SignalWithWeak<T, bool>
    {
    public:
        using Super = SignalWithWeak<T, bool>;
        using Super::Super;

        template <typename U, typename V, typename... Us>
        Signal<Conditional<T, Us...>, Us...> conditional(
                Signal<U, Us...> trueSignal,
                Signal<V, Us...> falseSignal) const
        {
            return wrap(Conditional<T, Us...>(
                        Super::sig_,
                        std::move(trueSignal),
                        std::move(falseSignal)
                        ));
        }

    };

    template <typename TStorage, typename... Ts>
    using SignalStorageType = std::conditional_t<
            std::is_same_v<void, TStorage>,
            SignalTypeless<Ts...>,
            TStorage
            >;

    template <typename TStorage, typename... Ts>
    class Signal : public SignalWithConditional<SignalStorageType<TStorage, Ts...>, Ts...>
    {
    public:
        using StorageType = std::conditional_t<
            std::is_same_v<void, TStorage>,
            SignalTypeless<Ts...>,
            TStorage
            >;

        using Super = SignalWithConditional<SignalStorageType<TStorage, Ts...>, Ts...>;
        using DataType = typename StorageType::DataType;

        Signal(Signal const&) = default;
        Signal(Signal&&) noexcept = default;

        Signal& operator=(Signal const&) = default;
        Signal& operator=(Signal&&) noexcept = default;

        Signal(StorageType sig) :
            Super(std::move(sig))
        {
        }

        StorageType& unwrap() &
        {
            return Super::sig_;
        }

        StorageType unwrap() &&
        {
            return std::move(Super::sig_);
        }

        StorageType const& unwrap() const&
        {
            return Super::sig_;
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
            return wrap(Shared<StorageType, Ts...>(Super::sig_));
        }

        auto join() const
        {
            return wrap(Join<TStorage>(Super::sig_));
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
                        std::move(Super::sig_)));
        }

        auto tee(InputHandle<Ts...> handle) const
        {
            auto shared = share();
            handle.set(shared.weak());
            return shared;
        }

        template <typename TFunc, typename... Us, typename = std::enable_if_t<
            std::is_assignable_v<SignalResult<Us...>, std::invoke_result_t<TFunc, Ts...>>
            >>
        auto tee(InputHandle<Us...> handle, TFunc&& func) const
        {
            auto mapped = this->map(std::forward<TFunc>(func)).share();
            handle.set(mapped.weak());

            // Store a reference to mapped in the lambda capture
            return mapped.map([mapped](auto&&... ts)
                {
                    return makeSignalResult(std::forward<decltype(ts)>(ts)...);
                });
        }
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

