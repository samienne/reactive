#pragma once

#include "widget.h"

#include <type_traits>

namespace reactive::widget
{
    template <typename TFunc>
    class WidgetModifier;

    using AnyWidgetModifier = WidgetModifier<
        std::function<AnySignal<Widget>(AnySignal<Widget>)>
        >;

    template <typename T>
    struct IsWidgetModifier : std::false_type {};

    template <typename T>
    struct IsWidgetModifier<WidgetModifier<T>> : std::true_type {};

    struct WidgetModifierBuildTag {};

    template <typename TFunc>
    class WidgetModifier
    {
    public:
        WidgetModifier(WidgetModifierBuildTag&&, TFunc&& func) :
            func_(std::move(func))
        {
        }

        WidgetModifier(WidgetModifier const&) = default;
        WidgetModifier(WidgetModifier&&) = default;

        WidgetModifier& operator=(WidgetModifier const&) = default;
        WidgetModifier& operator=(WidgetModifier&&) = default;

        template <typename U>
        auto operator()(Signal<U, Widget> widget)
        {
            return func_(std::move(widget));
        }

        operator AnyWidgetModifier() &&
        {
            return AnyWidgetModifier(
                    WidgetModifierBuildTag{},
                    std::move(func_)
                    );
        }

    private:
        TFunc func_;
    };

    template <typename T, typename U>
    auto operator|(Signal<T, Widget> w, WidgetModifier<U> t)
    {
        return std::move(std::move(t)(std::move(w)));
    }

    template <typename TFunc, typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Widget>, TFunc, AnySignal<Widget>
                 > > >
    auto makeWidgetSignalModifier(TFunc&& f)
    {
        return WidgetModifier<std::decay_t<TFunc>>(
                WidgetModifierBuildTag{},
                std::forward<TFunc>(f)
                );
    }

    template <typename TFunc, typename T,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Widget>, TFunc, AnySignal<Widget>, T
                 > > >
    auto makeWidgetSignalModifier(TFunc&& f, T&& t)
    {
        return makeWidgetSignalModifier(
            [f=std::forward<TFunc>(f), t=btl::cloneOnCopy(std::forward<T>(t))]
            (auto widget) mutable
            {
                return std::invoke(f, std::move(widget), std::move(*t));
            });
    }

    template <typename TFunc, typename T, typename U,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Widget>, TFunc, AnySignal<Widget>, T, U
                 > > >
    auto makeWidgetSignalModifier(TFunc&& f, T&& t, U&& u)
    {
        return makeWidgetSignalModifier(
            [f=std::forward<TFunc>(f), t=btl::cloneOnCopy(std::forward<T>(t)),
            u=btl::cloneOnCopy(std::forward<U>(u))]
            (auto widget) mutable
            {
                return std::invoke(f, std::move(widget), std::move(*t), std::move(*u));
            });
    }

    template <typename TFunc, typename T, typename U, typename V,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Widget>, TFunc, AnySignal<Widget>, T, U, V
                 > > >
    auto makeWidgetSignalModifier(TFunc&& f, T&& t, U&& u, V&& v)
    {
        return makeWidgetSignalModifier(
            [f=std::forward<TFunc>(f),
            t=btl::cloneOnCopy(std::forward<T>(t)),
            u=btl::cloneOnCopy(std::forward<U>(u)),
            v=btl::cloneOnCopy(std::forward<V>(v))]
            (auto widget) mutable
            {
                return std::invoke(f, std::move(widget), std::move(*t), std::move(*u),
                        std::move(*v));
            });
    }

    template <typename TFunc, typename T, typename U, typename V, typename W,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Widget>, TFunc, AnySignal<Widget>, T, U, V, W
                 > > >
    auto makeWidgetSignalModifier(TFunc&& f, T&& t, U&& u, V&& v, W&& w)
    {
        return makeWidgetSignalModifier(
            [f=std::forward<TFunc>(f),
            t=btl::cloneOnCopy(std::forward<T>(t)),
            u=btl::cloneOnCopy(std::forward<U>(u)),
            v=btl::cloneOnCopy(std::forward<V>(v)),
            w=btl::cloneOnCopy(std::forward<W>(w))]
            (auto widget) mutable
            {
                return std::invoke(f, std::move(widget), std::move(*t), std::move(*u),
                        std::move(*v), std::move(*w));
            });
    }

    template <typename TFunc, typename T, typename U, typename V, typename W,
             typename X,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Widget>, TFunc, AnySignal<Widget>, T, U, V, W, X
                 > > >
    auto makeWidgetSignalModifier(TFunc&& f, T&& t, U&& u, V&& v, W&& w, X&& x)
    {
        return makeWidgetSignalModifier(
            [f=std::forward<TFunc>(f),
            t=btl::cloneOnCopy(std::forward<T>(t)),
            u=btl::cloneOnCopy(std::forward<U>(u)),
            v=btl::cloneOnCopy(std::forward<V>(v)),
            w=btl::cloneOnCopy(std::forward<W>(w)),
            x=btl::cloneOnCopy(std::forward<X>(x))]
            (auto widget) mutable
            {
                return std::invoke(f, std::move(widget), std::move(*t), std::move(*u),
                        std::move(*v), std::move(*w), std::move(*x));
            });
    }

    template <typename TFunc, typename T, typename... Ts, typename = std::enable_if_t<
        std::is_convertible_v<
            TFunc,
            std::function<AnySignal<Widget>(AnySignal<Widget>, T, Ts...)>>
        >>
    auto makeWidgetSignalModifier(TFunc&& f, T&& t, Ts&&... ts)
    {
        return makeWidgetSignalModifier([f=std::forward<TFunc>(f),
                ts=std::make_tuple(std::forward<T>(t), std::forward<Ts>(ts)...)]
                    (auto widget) mutable
        {
            return std::apply([&](auto&&... ts) mutable
                    {
                        return std::invoke(
                                f,
                                std::move(widget),
                                std::forward<decltype(ts)>(ts)...);
                    },
                    std::move(ts)
                    );
        });
    }

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_convertible_v<
            TFunc,
            std::function<AnySignal<Widget>(AnySharedSignal<Widget>, Ts...)>>
        >>
    auto makeSharedWidgetSignalModifier(TFunc&& f, Ts&&... ts)
    {
        return makeWidgetSignalModifier([](auto widget, auto&& f, auto&&... ts)
            {
                return std::invoke(
                        std::forward<decltype(f)>(f),
                        share(std::move(widget)),
                        std::forward<decltype(ts)>(ts)...
                        );
            },
            std::forward<TFunc>(f),
            std::forward<Ts>(ts)...
            );
    }

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<Widget, TFunc, Widget, typename signal::SignalType<Ts>...>
        >>
    auto makeWidgetModifier(TFunc&& func, Ts&&... ts)
    {
        return makeWidgetSignalModifier([func=std::forward<TFunc>(func)]
            (auto widget, auto&&... ts) mutable
            {
                return signal::map(
                        func,
                        std::move(widget),
                        std::forward<decltype(ts)>(ts)...
                        );
            },
            std::forward<Ts>(ts)...
            );
    }

    inline auto makeEmptyWidgetModifier()
    {
        return makeWidgetSignalModifier([](auto widget)
                {
                    return widget;
                });
    }
} // namespace reactive::widget

