#pragma once

#include "instance.h"

#include <type_traits>

namespace reactive::widget
{
    template <typename TFunc>
    class InstanceModifier;

    using AnyInstanceModifier = InstanceModifier<
        std::function<AnySignal<Instance>(AnySignal<Instance>)>
        >;

    template <typename T>
    struct IsInstanceModifier : std::false_type {};

    template <typename T>
    struct IsInstanceModifier<InstanceModifier<T>> : std::true_type {};

    struct InstanceModifierBuildTag {};

    template <typename TFunc>
    class InstanceModifier
    {
    public:
        InstanceModifier(InstanceModifierBuildTag&&, TFunc&& func) :
            func_(std::move(func))
        {
        }

        InstanceModifier(InstanceModifier const&) = default;
        InstanceModifier(InstanceModifier&&) = default;

        InstanceModifier& operator=(InstanceModifier const&) = default;
        InstanceModifier& operator=(InstanceModifier&&) = default;

        template <typename U>
        auto operator()(Signal<U, Instance> instance)
        {
            return func_(std::move(instance));
        }

        operator AnyInstanceModifier() &&
        {
            return AnyInstanceModifier(
                    InstanceModifierBuildTag{},
                    std::move(func_)
                    );
        }

    private:
        TFunc func_;
    };

    template <typename T, typename U>
    auto operator|(Signal<T, Instance> w, InstanceModifier<U> t)
    {
        return std::move(std::move(t)(std::move(w)));
    }

    template <typename TFunc, typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Instance>, TFunc, AnySignal<Instance>
                 > > >
    auto makeInstanceSignalModifier(TFunc&& f)
    {
        return InstanceModifier<std::decay_t<TFunc>>(
                InstanceModifierBuildTag{},
                std::forward<TFunc>(f)
                );
    }

    template <typename TFunc, typename T,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Instance>, TFunc, AnySignal<Instance>, T
                 > > >
    auto makeInstanceSignalModifier(TFunc&& f, T&& t)
    {
        return makeInstanceSignalModifier(
            [f=std::forward<TFunc>(f), t=btl::cloneOnCopy(std::forward<T>(t))]
            (auto instance) mutable
            {
                return std::invoke(f, std::move(instance), std::move(*t));
            });
    }

    template <typename TFunc, typename T, typename U,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Instance>, TFunc, AnySignal<Instance>, T, U
                 > > >
    auto makeInstanceSignalModifier(TFunc&& f, T&& t, U&& u)
    {
        return makeInstanceSignalModifier(
            [f=std::forward<TFunc>(f), t=btl::cloneOnCopy(std::forward<T>(t)),
            u=btl::cloneOnCopy(std::forward<U>(u))]
            (auto instance) mutable
            {
                return std::invoke(f, std::move(instance), std::move(*t), std::move(*u));
            });
    }

    template <typename TFunc, typename T, typename U, typename V,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Instance>, TFunc, AnySignal<Instance>, T, U, V
                 > > >
    auto makeInstanceSignalModifier(TFunc&& f, T&& t, U&& u, V&& v)
    {
        return makeInstanceSignalModifier(
            [f=std::forward<TFunc>(f),
            t=btl::cloneOnCopy(std::forward<T>(t)),
            u=btl::cloneOnCopy(std::forward<U>(u)),
            v=btl::cloneOnCopy(std::forward<V>(v))]
            (auto instance) mutable
            {
                return std::invoke(f, std::move(instance), std::move(*t), std::move(*u),
                        std::move(*v));
            });
    }

    template <typename TFunc, typename T, typename U, typename V, typename W,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Instance>, TFunc, AnySignal<Instance>, T, U, V, W
                 > > >
    auto makeInstanceSignalModifier(TFunc&& f, T&& t, U&& u, V&& v, W&& w)
    {
        return makeInstanceSignalModifier(
            [f=std::forward<TFunc>(f),
            t=btl::cloneOnCopy(std::forward<T>(t)),
            u=btl::cloneOnCopy(std::forward<U>(u)),
            v=btl::cloneOnCopy(std::forward<V>(v)),
            w=btl::cloneOnCopy(std::forward<W>(w))]
            (auto instance) mutable
            {
                return std::invoke(f, std::move(instance), std::move(*t), std::move(*u),
                        std::move(*v), std::move(*w));
            });
    }

    template <typename TFunc, typename T, typename U, typename V, typename W,
             typename X,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Instance>, TFunc, AnySignal<Instance>, T, U, V, W, X
                 > > >
    auto makeInstanceSignalModifier(TFunc&& f, T&& t, U&& u, V&& v, W&& w, X&& x)
    {
        return makeInstanceSignalModifier(
            [f=std::forward<TFunc>(f),
            t=btl::cloneOnCopy(std::forward<T>(t)),
            u=btl::cloneOnCopy(std::forward<U>(u)),
            v=btl::cloneOnCopy(std::forward<V>(v)),
            w=btl::cloneOnCopy(std::forward<W>(w)),
            x=btl::cloneOnCopy(std::forward<X>(x))]
            (auto instance) mutable
            {
                return std::invoke(f, std::move(instance), std::move(*t), std::move(*u),
                        std::move(*v), std::move(*w), std::move(*x));
            });
    }

    template <typename TFunc, typename T, typename... Ts, typename = std::enable_if_t<
        std::is_convertible_v<
            TFunc,
            std::function<AnySignal<Instance>(AnySignal<Instance>, T, Ts...)>>
        >>
    auto makeInstanceSignalModifier(TFunc&& f, T&& t, Ts&&... ts)
    {
        return makeInstanceSignalModifier([f=std::forward<TFunc>(f),
                ts=std::make_tuple(std::forward<T>(t), std::forward<Ts>(ts)...)]
                    (auto instance) mutable
        {
            return std::apply([&](auto&&... ts) mutable
                    {
                        return std::invoke(
                                f,
                                std::move(instance),
                                std::forward<decltype(ts)>(ts)...);
                    },
                    std::move(ts)
                    );
        });
    }

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_convertible_v<
            TFunc,
            std::function<AnySignal<Instance>(AnySharedSignal<Instance>, Ts...)>>
        >>
    auto makeSharedInstanceSignalModifier(TFunc&& f, Ts&&... ts)
    {
        return makeInstanceSignalModifier([](auto instance, auto&& f, auto&&... ts)
            {
                return std::invoke(
                        std::forward<decltype(f)>(f),
                        share(std::move(instance)),
                        std::forward<decltype(ts)>(ts)...
                        );
            },
            std::forward<TFunc>(f),
            std::forward<Ts>(ts)...
            );
    }

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<Instance, TFunc, Instance, typename signal::SignalType<Ts>...>
        >>
    auto makeInstanceModifier(TFunc&& func, Ts&&... ts)
    {
        return makeInstanceSignalModifier([func=std::forward<TFunc>(func)]
            (auto instance, auto&&... ts) mutable
            {
                return signal::map(
                        func,
                        std::move(instance),
                        std::forward<decltype(ts)>(ts)...
                        );
            },
            std::forward<Ts>(ts)...
            );
    }

    inline auto makeEmptyInstanceModifier()
    {
        return makeInstanceSignalModifier([](auto instance)
                {
                    return instance;
                });
    }
} // namespace reactive::widget

