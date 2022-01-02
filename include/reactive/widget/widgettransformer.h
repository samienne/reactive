#pragma once

#include "reactive/widget.h"

#include <btl/cloneoncopy.h>

#include <functional>
#include <tuple>
#include <type_traits>

namespace reactive::widget
{
    template <typename... Ts>
    using WidgetTransformerFunc = std::function<
        std::pair<AnySignal<Widget>, btl::CloneOnCopy<std::tuple<Ts...>>>(AnySignal<Widget>)
        >;

    template <typename TFunc, typename... Ts>
    class WidgetTransformer;

    template <typename TFunc>
    auto makeWidgetTransformer(TFunc&& func);

    inline auto makeWidgetTransformer();

    namespace detail
    {
        template <typename T, typename...>
        struct GetWidgetTransformerFuncType
        {
            using type = T;
        };

        template <typename... Ts>
        struct GetWidgetTransformerFuncType<void, Ts...>
        {
            // void means type erasure
            using type = WidgetTransformerFunc<Ts...>;
        };


        template <typename T>
        inline constexpr bool isPair = false;

        template <typename T, typename U>
        inline constexpr bool isPair<std::pair<T, U>> = true;

        struct WidgetTransformerBuildTag {};

    } // namespace detail

    template <typename T>
    struct IsWidgetTransformer : std::false_type {};

    template <typename T, typename... Ts>
    struct IsWidgetTransformer<WidgetTransformer<T, Ts...>> : std::true_type {};

    template <typename TFunc, typename... Ts>
    class WidgetTransformer
    {
        using FuncType = typename detail::GetWidgetTransformerFuncType<
            TFunc, Ts...
            >::type;

    public:
        WidgetTransformer(detail::WidgetTransformerBuildTag&&, FuncType func) :
            func_(std::move(func))
        {
        }

        WidgetTransformer(WidgetTransformer const&) = default;
        WidgetTransformer(WidgetTransformer&&) = default;

        WidgetTransformer& operator=(WidgetTransformer const&) = default;
        WidgetTransformer& operator=(WidgetTransformer&&) = default;

        template <typename U>
        auto operator()(Signal<U, Widget> widget)
        {
            return func_(std::move(widget));
        }

        template <typename U>
        auto bind(U&& u) && // std::function<WidgetTransformer(Ts...)>
        {
            using UReturnType = std::invoke_result_t<U, Ts...>;
            static_assert(IsWidgetTransformer<UReturnType>::value);

            return makeWidgetTransformer(
                [f=std::move(func_), u=std::forward<U>(u)](auto w) mutable
                {
                    auto pair = std::move(f)(std::move(w));
                    auto transform = std::apply(std::move(u), std::move(*pair.second));

                    return std::move(transform)(std::move(pair.first));
                });
        }

        template <typename U, typename... Us>
        auto compose(WidgetTransformer<U, Us...> u) &&
        {
            static_assert(std::is_invocable_v<U, AnySignal<Widget>>);
            using UReturnType = std::invoke_result_t<U, AnySignal<Widget>>;
            static_assert(detail::isPair<UReturnType>);

            return makeWidgetTransformer(
                [f=std::move(func_), u=std::move(u)](auto w) mutable
                {
                    auto pair1 = std::move(f)(std::move(w));
                    auto pair2 = std::move(u)(std::move(pair1.first));

                    return std::make_pair(
                            std::move(pair2.first),
                            btl::cloneOnCopy(std::tuple_cat(
                                std::move(*pair1.second),
                                std::move(*pair2.second)
                                )
                            ));
                });
        }

        template <typename U, typename... Us, typename... Ws>
        auto compose(WidgetTransformer<U, Us...> u, Ws&&... ws) &&
        {
            return std::move(*this)
                .compose(std::move(u))
                .compose(std::forward<Ws>(ws)...)
                ;
        }

        template <typename... Us>
        auto values(Us&&... us) &&
        {
            return makeWidgetTransformer(
                [f=std::move(func_),
                us=btl::cloneOnCopy(std::make_tuple(std::forward<Us>(us)...))]
                (auto w) mutable
                {
                    auto pair = std::move(f)(std::move(w));

                    return std::make_pair(
                            std::move(pair.first),
                            btl::cloneOnCopy(std::tuple_cat(
                                std::move(*pair.second),
                                std::move(*us)
                                ))
                            );
                });
        }

        operator WidgetTransformer<void, Ts...>() &&
        {
            return WidgetTransformer<void>(
                    detail::WidgetTransformerBuildTag(),
                    std::move(func_)
                    );
        }

    private:
        FuncType func_;
    };

    namespace detail
    {
        template <typename TFunc, typename T>
        struct GetWidgetTransformerType {};

        template <typename TFunc, typename T, typename... Ts>
        struct GetWidgetTransformerType<
            TFunc,
            std::pair<T, btl::CloneOnCopy<std::tuple<Ts...>>>
            >
        {
            using type = WidgetTransformer<TFunc, Ts...>;
        };
    }

    template <typename TFunc>
    auto makeWidgetTransformer(TFunc&& func)
    {
        using ResultType = std::invoke_result_t<TFunc, signal::AnySignal<Widget>>;
        using WidgetTransformerType = typename detail::GetWidgetTransformerType<
            std::decay_t<TFunc>,
            ResultType
            >::type;

        return WidgetTransformerType(
                detail::WidgetTransformerBuildTag(),
                std::forward<TFunc>(func)
                );
    }

    inline auto makeWidgetTransformer()
    {
        return makeWidgetTransformer([](auto w)
            {
                return std::make_pair(
                        std::move(w),
                        btl::cloneOnCopy(std::tuple<>())
                        );
            });
    }

    template <typename... Ts>
    auto provideValues(Ts&&... ts)
    {
        return makeWidgetTransformer(
            [ts=btl::cloneOnCopy(std::make_tuple(std::forward<Ts>(ts)...))]
            (auto w) mutable
            {
                return std::make_pair(
                        std::move(w),
                        std::move(ts)
                        );
            });
    }


    template <typename TWidget, typename... Ts>
    auto makeWidgetTransformerResult(signal::Signal<TWidget, Widget> widget, Ts&&... ts)
    {
        return std::make_pair(
                std::move(widget),
                btl::cloneOnCopy(std::make_tuple(std::forward<Ts>(ts)...))
                );
    }

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

    template <typename T, typename... Us>
    auto operator|(Signal<T, Widget> w, WidgetTransformer<Us...> t)
    {
        return std::move(std::move(t)(std::move(w)).first);
    }
} // reactive::widget

