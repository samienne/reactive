#include <tuple>
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
#include "btl/forcenoexcept.h"
#include "btl/typelist.h"

#include <mutex>
#include <utility>
#include <type_traits>

namespace reactive::signal
{
    template <typename T2> class Weak;

    template <typename TStorage, typename... Ts>
    class SharedSignal;

    namespace detail
    {
        template <typename T>
        struct GetSignalResultTypesHelper
        {
            using type = btl::TypeList<T>;
        };

        template <typename... Ts>
        struct GetSignalResultTypesHelper<SignalResult<Ts...>>
        {
            using type = btl::TypeList<Ts...>;
        };

        template <template <typename> class TPredicate, typename T>
        struct AreAll : btl::All<TPredicate<T>>
        {
        };

        template <template <typename> class TPredicate, typename... Ts>
        struct AreAll<TPredicate, btl::TypeList<Ts...>> :
            btl::All<TPredicate<Ts>...>
        {
        };

        template <template <typename> class TPredicate, typename T>
        struct IsAny : btl::All<TPredicate<T>>
        {
        };

        template <template <typename> class TPredicate, typename... Ts>
        struct IsAny<TPredicate, btl::TypeList<Ts...>> :
            btl::Any<TPredicate<Ts>...>
        {
        };
    }

    template <typename T>
    struct GetSignalResultTypes
    {
        using type = detail::GetSignalResultTypesHelper<SignalType<T>>;
    };

    template <typename TFunc, typename TStorage>
    class Map
    {
    public:
        Map(TFunc func, TStorage storage) :
            func_(std::move(func)),
            storage_(std::move(storage))
        {
        }

        Map(Map&& other) noexcept = default;

        auto evaluate() const -> decltype(auto)
        {
            if constexpr(IsSignalResult<decltype(storage_->evaluate())>::value)
            {
                using ReturnType = decltype(
                        std::apply(*func_, storage_->evaluate().getTuple())
                        );
                using ReturnTypes = typename detail::GetSignalResultTypesHelper<
                    ReturnType>::type;

                if constexpr(!detail::AreAll<
                        std::is_reference, typename GetSignalResultTypes<TStorage>::type
                        >::value && detail::IsAny<std::is_reference, ReturnTypes>::value)
                {
                    return btl::clone(std::apply(*func_, storage_->evaluate().getTuple()));
                }
                else
                {
                    return std::apply(*func_, storage_->evaluate().getTuple());
                }
            }
            else
            {
                using ReturnType = decltype(std::invoke(*func_, storage_->evaluate()));

                if constexpr(!std::is_reference_v<SignalType<TStorage>> &&
                        std::is_reference_v<ReturnType>
                        )
                {
                    return btl::clone(std::invoke(*func_, storage_->evaluate()));
                }
                else
                {
                    return std::invoke(*func_, storage_->evaluate());
                }
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

        Map clone() const
        {
            return *this;
        }

    private:
        Map(Map const&) = default;

        mutable btl::ForceNoexcept<TFunc> func_;
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
            return wrap(Map<std::decay_t<TFunc>, StorageType>(
                    std::forward<TFunc>(f),
                    std::move(*sig_)
                    ));
        }

        template <typename TFunc>
        auto mapToFunction(TFunc&& f) &&
        {
            return std::move(*this).map([f=std::forward<TFunc>(f)](auto&&... ts) mutable
                    {
                        return [f,
                        args=std::make_tuple(std::forward<decltype(ts)>(ts)...)]
                        (auto&&... us) mutable
                        -> std::invoke_result_t<
                            decltype(f),
                            decltype(ts)...,
                            decltype(us)...
                            >
                        {
                            return std::apply(
                                    [&f, &us...](auto&&... vs) mutable -> decltype(auto)
                                    {
                                        return std::invoke(
                                                f,
                                                std::forward<decltype(vs)>(vs)...,
                                                std::forward<decltype(us)>(us)...
                                                );
                                    },
                                    args
                                    );
                        };
                    });
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

    template <typename... Ts>
    class AnySignal : public Signal<void, Ts...>
    {
    public:
        template <typename U, typename... Vs> friend class Signal;

        using NestedSignalType = void;

        template <typename... Us, typename TStorage, typename =
            std::enable_if_t<
                btl::All<
                    std::is_convertible<std::decay_t<Us>, std::decay_t<Ts>>...,
                    btl::Any<
                        btl::Not<std::is_reference<Ts>>,
                        btl::All<std::is_reference<Ts>, std::is_reference<Us>>
                    >...
                >::value
            >>
        AnySignal(Signal<TStorage, Us...>&& other) :
            Signal<void, Ts...>(typed<Ts...>(std::move(other)))
        {
        }

        template <typename UStorage>
        AnySignal(SharedSignal<UStorage, Ts...> other) :
            Signal<void, Ts...>(std::move(other).storage().ptr())
        {
        }

        template <typename... Us, typename UStorage, typename =
            std::enable_if_t<
                btl::All<
                    std::is_convertible<std::decay_t<Us>, std::decay_t<Ts>>...,
                    btl::Any<
                        btl::Not<std::is_reference<Ts>>,
                        btl::All<std::is_reference<Ts>, std::is_reference<Us>>
                    >...
                >::value
            >>
        AnySignal(SharedSignal<UStorage, Us...> other) :
            Signal<void, Ts...>(typed<Ts...>(std::move(other)))
        {
        }

    protected:
        AnySignal(AnySignal const&) = default;
        AnySignal<Ts...>& operator=(AnySignal const&) = default;

    public:
        AnySignal(AnySignal&&) noexcept = default;
        AnySignal<Ts...>& operator=(AnySignal&&) noexcept = default;

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

