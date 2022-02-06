#pragma once

#include "buildparams.h"
#include "buildermodifier.h"

#include "reactive/signal/constant.h"
#include "reactive/signal/signal.h"

#include <btl/pushback.h>
#include <btl/bindarguments.h>

#include <optional>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <typeindex>
#include <any>
#include <utility>

namespace reactive::widget
{
    struct WidgetBuildTag {};

    template <typename TFunc>
    class Widget;

    using AnyWidget = Widget<std::function<AnyBuilder(BuildParams)>>;

    template <typename TFunc>
    class WidgetModifier;

    using AnyWidgetModifier = WidgetModifier<std::function<AnyWidget(AnyWidget)>>;

    template <typename TFunc>
    class Widget
    {
    public:
        Widget(WidgetBuildTag const&, TFunc func) :
            func_(std::move(func))
        {
        }

        Widget(Widget const&) = default;
        Widget(Widget&&) noexcept = default;

        Widget& operator=(Widget const&) = default;
        Widget& operator=(Widget&&) noexcept = default;

        auto operator()(BuildParams params) &&
        {
            return std::invoke(std::move(*func_), std::move(params));
        }

        operator AnyWidget() &&
        {
            return AnyWidget(
                    WidgetBuildTag{},
                    std::move(*func_)
                    );
        }

        template <typename T>
        auto operator|(WidgetModifier<T> modifier) &&
        {
            return std::invoke(
                    std::move(modifier),
                    std::move(*this)
                    );
        }

        auto clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<TFunc> func_;
    };

    extern template class Widget<std::function<AnyBuilder(BuildParams)>>;

    namespace detail
    {
        template <typename TFunc>
        auto makeWidgetUnchecked(TFunc&& func)
        {
            return Widget<std::decay_t<TFunc>>(
                    WidgetBuildTag{},
                    std::forward<TFunc>(func)
                    );
        }

        template <typename TFunc, typename... Ts>
        auto makeWidgetUnchecked(TFunc&& func, Ts&&... ts)
        {
            return makeWidgetUnchecked(btl::bindArguments(
                        [](BuildParams params, auto&& func, auto&&... ts)
                        {
                            return std::invoke(
                                    std::forward<decltype(func)>(func),
                                    std::move(params),
                                    std::forward<decltype(ts)>(ts)...
                                    );
                        },
                        std::forward<TFunc>(func),
                        std::forward<Ts>(ts)...
                        )
                    );
        }
    } // namespace detail

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyBuilder, TFunc, BuildParams, Ts&&...>
        >
    >
    auto makeWidget(TFunc&& func, Ts&&... ts)
    {
        return detail::makeWidgetUnchecked(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyWidget, TFunc, BuildParams, Ts&&...>
        >, int = 0>
    auto makeWidget(TFunc&& func, Ts&&... ts)
    {
        return detail::makeWidgetUnchecked([](BuildParams const& params,
                    auto func, auto&&... ts)
            {
                return std::invoke(
                    std::invoke(
                        std::move(func),
                        params,
                        std::forward<decltype(ts)>(ts)...
                        ),
                    params
                    );
            },
            std::forward<TFunc>(func),
            std::forward<Ts>(ts)...
            );
    }

    inline auto makeWidget()
    {
        return detail::makeWidgetUnchecked([](auto&& params)
            {
                return makeBuilder()
                    .setBuildParams(std::forward<decltype(params)>(params))
                    ;
            });
    }

    struct WidgetModifierBuildTag {};

    template <typename TFunc>
    class WidgetModifier
    {
    public:
        WidgetModifier(WidgetModifierBuildTag const&, TFunc func) :
            func_(std::move(func))
        {
        }

        WidgetModifier(WidgetModifier const&) = default;
        WidgetModifier(WidgetModifier&&) noexcept = default;

        WidgetModifier& operator=(WidgetModifier const&) = default;
        WidgetModifier& operator=(WidgetModifier&&) noexcept = default;

        template <typename T>
        auto operator()(Widget<T> widget) &&
        {
            return std::invoke(std::move(*func_), std::move(widget));
        }

        operator AnyWidgetModifier() &&
        {
            return AnyWidgetModifier(WidgetModifierBuildTag{}, std::move(*func_));
        }

    private:
        btl::CloneOnCopy<TFunc> func_;
    };

    extern template class WidgetModifier<std::function<AnyWidget(AnyWidget)>>;

    namespace detail
    {
        template <typename TFunc>
        auto makeWidgetModifierUnchecked(TFunc&& func)
        {
            return WidgetModifier<std::decay_t<TFunc>>(
                    WidgetModifierBuildTag{},
                    std::forward<TFunc>(func)
                    );
        }

        template <typename TFunc, typename... Ts>
        auto makeWidgetModifierUnchecked(TFunc&& func, Ts&&... ts)
        {
            return makeWidgetModifierUnchecked(btl::bindArguments(
                        [](auto widget, auto&& func, auto&&... ts)
                        {
                            return std::invoke(
                                    std::forward<decltype(func)>(func),
                                    std::move(widget),
                                    std::forward<decltype(ts)>(ts)...
                                    );
                        },
                        std::forward<TFunc>(func),
                        std::forward<Ts>(ts)...
                        )
                    );
        }
    } // namespace detail

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyWidget, TFunc, AnyWidget, Ts&&...>
        >
    >
    auto makeWidgetModifier(TFunc&& func, Ts&&... ts)
    {
        return detail::makeWidgetModifierUnchecked(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }

    template <typename T>
    auto makeWidgetModifier(BuilderModifier<T> modifier)
    {
        return detail::makeWidgetModifierUnchecked([](auto widget, auto modifier)
                {
                    return detail::makeWidgetUnchecked([
                            ](BuildParams params, auto widget, auto modifier) mutable
                            {
                                return std::invoke(
                                        std::move(widget),
                                        std::move(params)
                                        )
                                    | std::move(modifier)
                                    ;
                            },
                            std::move(widget),
                            std::move(modifier)
                            );
                },
                std::move(modifier)
                );
    }

    template <typename T>
    auto makeWidgetModifier(ElementModifier<T> modifier)
    {
        return detail::makeWidgetModifierUnchecked([](auto widget, auto modifier)
                {
                    return detail::makeWidgetUnchecked([
                            ](BuildParams params, auto widget, auto modifier) mutable
                            {
                                return std::invoke(
                                        std::move(widget),
                                        std::move(params)
                                        )
                                    | makeBuilderModifier(std::move(modifier))
                                    ;
                            },
                            std::move(widget),
                            std::move(modifier)
                            );
                },
                std::move(modifier)
                );
    }


    template <typename T>
    auto makeWidgetModifier(InstanceModifier<T> modifier)
    {
        return makeWidgetModifier([](auto widget, auto modifier)
                {
                    return detail::makeWidgetUnchecked([](BuildParams params,
                                auto widget, auto modifier)
                            {
                                return std::invoke(
                                        std::move(widget),
                                        std::move(params)
                                        )
                                    | makeBuilderModifier(std::move(modifier))
                                    ;
                            },
                            std::move(widget),
                            std::move(modifier));
                },
                std::move(modifier)
                );
    }

    template <typename T, typename U>
    auto operator|(Widget<T>&& widget, BuilderModifier<U> modifier)
    {
        return std::move(widget)
            | makeWidgetModifier(std::move(modifier))
            ;
    }

    template <typename T, typename U>
    auto operator|(Widget<T>&& widget, ElementModifier<U> modifier)
    {
        return std::move(widget)
            | makeWidgetModifier(std::move(modifier))
            ;
    }

    template <typename T, typename U>
    auto operator|(Widget<T>&& widget, InstanceModifier<U> modifier)
    {
        return std::move(widget)
            | makeWidgetModifier(std::move(modifier))
            ;
    }
} // namespace reactive::widget

