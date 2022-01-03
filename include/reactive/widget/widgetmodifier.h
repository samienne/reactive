#pragma once

#include "widgettransformer.h"

namespace reactive::widget
{
    template <typename TFunc, typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Widget>, TFunc, AnySignal<Widget>
                 > > >
    auto makeWidgetSignalModifier(TFunc&& f)
    {
        return makeWidgetTransformer([f=std::forward<TFunc>(f)](auto widget) mutable
        {
            return makeWidgetTransformerResult(
                    std::invoke(f, std::move(widget))
                    );
        });
    }

    template <typename TFunc, typename T,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Widget>, TFunc, AnySignal<Widget>, T
                 > > >
    auto makeWidgetSignalModifier(TFunc&& f, T&& t)
    {
        return makeWidgetTransformer(
            [f=std::forward<TFunc>(f), t=btl::cloneOnCopy(std::forward<T>(t))]
            (auto widget) mutable
            {
                return makeWidgetTransformerResult(
                        std::invoke(f, std::move(widget), std::move(*t))
                        );
            });
    }

    template <typename TFunc, typename T, typename U,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Widget>, TFunc, AnySignal<Widget>, T, U
                 > > >
    auto makeWidgetSignalModifier(TFunc&& f, T&& t, U&& u)
    {
        return makeWidgetTransformer(
            [f=std::forward<TFunc>(f), t=btl::cloneOnCopy(std::forward<T>(t)),
            u=btl::cloneOnCopy(std::forward<U>(u))]
            (auto widget) mutable
            {
                return makeWidgetTransformerResult(
                        std::invoke(f, std::move(widget), std::move(*t), std::move(*u))
                        );
            });
    }

    template <typename TFunc, typename T, typename U, typename V,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Widget>, TFunc, AnySignal<Widget>, T, U, V
                 > > >
    auto makeWidgetSignalModifier(TFunc&& f, T&& t, U&& u, V&& v)
    {
        return makeWidgetTransformer(
            [f=std::forward<TFunc>(f),
            t=btl::cloneOnCopy(std::forward<T>(t)),
            u=btl::cloneOnCopy(std::forward<U>(u)),
            v=btl::cloneOnCopy(std::forward<V>(v))]
            (auto widget) mutable
            {
                return makeWidgetTransformerResult(
                        std::invoke(f, std::move(widget), std::move(*t), std::move(*u),
                            std::move(*v))
                        );
            });
    }

    template <typename TFunc, typename T, typename U, typename V, typename W,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Widget>, TFunc, AnySignal<Widget>, T, U, V, W
                 > > >
    auto makeWidgetSignalModifier(TFunc&& f, T&& t, U&& u, V&& v, W&& w)
    {
        return makeWidgetTransformer(
            [f=std::forward<TFunc>(f),
            t=btl::cloneOnCopy(std::forward<T>(t)),
            u=btl::cloneOnCopy(std::forward<U>(u)),
            v=btl::cloneOnCopy(std::forward<V>(v)),
            w=btl::cloneOnCopy(std::forward<W>(w))]
            (auto widget) mutable
            {
                return makeWidgetTransformerResult(
                        std::invoke(f, std::move(widget), std::move(*t), std::move(*u),
                            std::move(*v), std::move(*w))
                        );
            });
    }

    template <typename TFunc, typename T, typename U, typename V, typename W,
             typename X,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Widget>, TFunc, AnySignal<Widget>, T, U, V, W, X
                 > > >
    auto makeWidgetSignalModifier(TFunc&& f, T&& t, U&& u, V&& v, W&& w, X&& x)
    {
        return makeWidgetTransformer(
            [f=std::forward<TFunc>(f),
            t=btl::cloneOnCopy(std::forward<T>(t)),
            u=btl::cloneOnCopy(std::forward<U>(u)),
            v=btl::cloneOnCopy(std::forward<V>(v)),
            w=btl::cloneOnCopy(std::forward<W>(w)),
            x=btl::cloneOnCopy(std::forward<X>(x))]
            (auto widget) mutable
            {
                return makeWidgetTransformerResult(
                        std::invoke(f, std::move(widget), std::move(*t), std::move(*u),
                            std::move(*v), std::move(*w), std::move(*x))
                        );
            });
    }

    template <typename TFunc, typename T, typename... Ts, typename = std::enable_if_t<
        std::is_convertible_v<
            TFunc,
            std::function<AnySignal<Widget>(AnySignal<Widget>, T, Ts...)>>
        >>
    auto makeWidgetSignalModifier(TFunc&& f, T&& t, Ts&&... ts)
    {
        return makeWidgetTransformer([f=std::forward<TFunc>(f),
                ts=std::make_tuple(std::forward<T>(t), std::forward<Ts>(ts)...)]
                    (auto widget) mutable
        {
            return std::apply([&](auto&&... ts) mutable
                    {
                        return makeWidgetTransformerResult(
                                std::invoke(
                                    f,
                                    std::move(widget),
                                    std::forward<decltype(ts)>(ts)...)
                                );
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
} // namespace reactive::widget

