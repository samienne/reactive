#pragma once

#include "instance.h"

#include <btl/bindarguments.h>

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

    namespace detail
    {
        template <typename TFunc>
        auto makeInstanceSignalModifierUnchecked(TFunc&& f)
        {
            return InstanceModifier<std::decay_t<TFunc>>(
                    InstanceModifierBuildTag{},
                    std::forward<TFunc>(f)
                    );
        }

        template <typename TFunc, typename... Ts>
        auto makeInstanceSignalModifierUnchecked(TFunc&& f, Ts&&... ts)
        {
            return makeInstanceSignalModifierUnchecked(
                    btl::bindArguments(
                        [](auto&& instance, auto&& func, auto&&... ts)
                        {
                            return std::invoke(
                                    std::forward<decltype(func)>(func),
                                    std::forward<decltype(instance)>(instance),
                                    std::forward<decltype(ts)>(ts)...
                                    );
                        },
                        std::forward<TFunc>(f),
                        std::forward<Ts>(ts)...
                        )
                    );
        }
    } // namespace detail

    template <typename TFunc, typename... Ts,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 AnySignal<Instance>, TFunc, AnySignal<Instance>, Ts&&...
                 > > >
    auto makeInstanceSignalModifier(TFunc&& func, Ts&&... ts)
    {
        return detail::makeInstanceSignalModifierUnchecked(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
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

