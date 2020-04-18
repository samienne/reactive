#pragma once

#include "frameinfo.h"
#include "typed.h"
#include "signalbase.h"
#include "signaltraits.h"
#include "signalresult.h"

#include <btl/cloneoncopy.h>
#include <btl/shared.h>
#include <btl/spinlock.h>
#include <btl/demangle.h>
#include <btl/not.h>
#include <btl/all.h>
#include <btl/any.h>

#include <mutex>
#include <utility>
#include <type_traits>

namespace reactive::signal
{
    template <typename T2> class Weak;

    template <typename TStorage, typename... Ts>
    class SharedSignal;

    template <typename TFunc, typename TStorage>
    class Map3
    {
    public:
        Map3(TFunc func, TStorage storage) :
            func_(std::move(func)),
            storage_(std::move(storage))
        {
        }

        Map3(Map3&& other) noexcept = default;

        auto evaluate() const -> decltype(auto)
        {
            if constexpr(IsSignalResult<decltype(storage_->evaluate())>::value)
            {
                return std::apply(*func_, storage_->evaluate().getTuple());
            }
            else
            {
                return (*func_)(storage_->evaluate());
            }
        }

        bool hasChanged() const
        {
            return storage_->hasChanged();
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            return storage_->updateBegin(frame);
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            return storage_->updateEnd(frame);
        }

        template <typename TCallback>
        btl::connection observe(TCallback&& callback)
        {
            return storage_->observe(std::forward<TCallback>(callback));
        }

        Annotation annotate() const
        {
            Annotation a;
            //auto&& n = a.addNode("Signal<" + btl::demangle<T>()
                    //+ "> changed: " + std::to_string(hasChanged()));
            //a.addShared(sig_.raw_ptr(), n, deferred_->annotate());
            return a;
        }

        Map3 clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<TFunc> func_;
        btl::CloneOnCopy<TStorage> storage_;
    };

    template <typename TStorage, typename... Ts> // defined in typed.h
    class Signal
    {
    public:
        template <typename U, typename... Vs> friend class Signal;

        using NestedSignalType = TStorage;
        using StorageType = std::conditional_t<std::is_same_v<void, TStorage>,
              Share<SignalBase<Ts...>, Ts...>,
              TStorage
            >;

        Signal(StorageType sig) :
            sig_(std::move(sig))
        {
        }

        Signal(SharedSignal<TStorage, Ts...>&& other) noexcept:
            sig_(std::move(other).storage())
        {
        }

        Signal(SharedSignal<TStorage, Ts...>& other) :
            sig_(btl::clone(other.storage()))
        {
        }

        Signal(SharedSignal<TStorage, Ts...> const& other) :
            sig_(btl::clone(other.storage()))
        {
        }

        template <typename UStorage, typename = std::enable_if_t<
            std::is_base_of<Signal, SharedSignal<UStorage, Ts...>>::value
            >>
        Signal(SharedSignal<UStorage, Ts...> const& other) :
            sig_(btl::clone(other.storage()))
        {
        }

        template <typename UStorage, typename = std::enable_if_t<
            std::is_base_of<Signal, SharedSignal<UStorage, Ts...>>::value
            >>
        Signal(SharedSignal<UStorage, Ts...>&& other) :
            sig_(std::move(other).storage())
        {
        }

        template <typename UStorage, typename = std::enable_if_t<
            std::is_base_of<Signal, SharedSignal<UStorage, Ts...>>::value
            >>
        Signal(SharedSignal<UStorage, Ts...>& other) :
            sig_(other.storage())
        {
        }

    protected:
        Signal(Signal const&) = default;
        Signal& operator=(Signal const&) = default;
        Signal& operator=(Signal&&) noexcept = default;

    public:
        Signal(Signal&&) noexcept = default;

        auto evaluate() const -> decltype(auto)
        {
            return sig_->evaluate();
        }

        bool hasChanged() const
        {
            return sig_->hasChanged();
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            return sig_->updateBegin(frame);
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            return sig_->updateEnd(frame);
        }

        template <typename TCallback>
        btl::connection observe(TCallback&& callback)
        {
            return sig_->observe(std::forward<TCallback>(callback));
        }

        Annotation annotate() const
        {
            Annotation a;
            //auto&& n = a.addNode("Signal<" + btl::demangle<T>()
                    //+ "> changed: " + std::to_string(hasChanged()));
            //a.addShared(sig_.raw_ptr(), n, deferred_->annotate());
            return a;
        }

        Signal clone() const
        {
            return *this;
        }

        StorageType const& storage() const &
        {
            return *sig_;
        }

        StorageType&& storage() &&
        {
            return std::move(*sig_);
        }

        bool isCached() const
        {
            if constexpr(std::is_same_v<void, TStorage>)
            {
                return Signal<void, Ts...>::storage().ptr()->isCached();
            }
            else
            {
                return std::is_reference_v<typename SignalType<StorageType>::type>;
            }
        }

        template <typename TFunc>
        auto map(TFunc&& f) &&
        {
            return wrap(Map3<std::decay_t<TFunc>, StorageType>(
                    std::forward<TFunc>(f),
                    std::move(*sig_)
                    ));
        }

    private:
        template <typename T2> friend class Weak;
        btl::CloneOnCopy<StorageType> sig_;
    };

    template <typename TStorage, typename TResult>
    struct GetSignalTypeFromResult
    {
        using type = Signal<TStorage, std::decay_t<TResult>>;
    };

    template <typename TStorage, typename... Ts>
    struct GetSignalTypeFromResult<TStorage, SignalResult<Ts...>>
    {
        using type = Signal<TStorage, std::decay_t<Ts>...>;
    };

    template <typename TStorage, typename = std::enable_if_t<
        btl::All<
            std::is_convertible<TStorage, std::decay_t<TStorage>>,
            IsSignal<TStorage>
        >::value
        >>
    //Signal<std::decay_t<TStorage>, std::decay_t<SignalType<TStorage>>>
    typename GetSignalTypeFromResult<TStorage, std::decay_t<SignalType<TStorage>>>::type
    wrap(TStorage&& sig)
    {
        return { std::forward<TStorage>(sig) };
    }

    template <typename... Ts, typename TStorage, typename = std::enable_if_t<
        IsSignal<TStorage>::value
        >>
    auto wrap(Signal<TStorage, Ts...>&& sig)
    {
        return std::move(sig);
    }

    template <typename T>
    class AnySignal : public Signal<void, T>
    {
    public:
        template <typename U, typename... Vs> friend class Signal;

        using NestedSignalType = void;

        template <typename U, typename TStorage, typename =
            std::enable_if_t<
                btl::All<
                    std::is_convertible<std::decay_t<U>, std::decay_t<T>>,
                    btl::Any<
                        btl::Not<std::is_reference<T>>,
                        btl::All<std::is_reference<T>, std::is_reference<U>>
                    >
                >::value
            >>
        AnySignal(Signal<TStorage, U>&& other) :
            Signal<void, T>(typed<T>(std::move(other)))
        {
        }

        template <typename UStorage>
        AnySignal(SharedSignal<UStorage, T> other) :
            Signal<void, T>(std::move(other).storage().ptr())
        {
        }

        template <typename U, typename UStorage, typename =
            std::enable_if_t<
                btl::All<
                    std::is_convertible<std::decay_t<U>, std::decay_t<T>>,
                    btl::Any<
                        btl::Not<std::is_reference<T>>,
                        btl::All<std::is_reference<T>, std::is_reference<U>>
                    >
                >::value
            >>
        AnySignal(SharedSignal<UStorage, U> other) :
            Signal<void, T>(typed<T>(std::move(other)))
        {
        }

    protected:
        AnySignal(AnySignal const&) = default;
        AnySignal<T>& operator=(AnySignal const&) = default;

    public:
        AnySignal(AnySignal&&) noexcept = default;
        AnySignal<T>& operator=(AnySignal&&) noexcept = default;

        AnySignal clone() const
        {
            return *this;
        }

    private:
        template <typename T2> friend class Weak;
    };

} // namespace reactive::signal

namespace reactive
{
    template <typename T, typename U>
    using Signal = signal::Signal<T, U>;

    template <typename T, typename U>
    using SharedSignal = signal::SharedSignal<T, U>;

    template <typename T>
    using AnySignal = signal::AnySignal<T>;
} // namespace reactive

