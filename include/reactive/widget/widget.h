#pragma once

#include "buildermodifier.h"

#include "reactive/signal/constant.h"
#include "reactive/signal/signal.h"

#include <btl/pushback.h>

#include <optional>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <typeindex>
#include <any>

namespace reactive::widget
{
    class BuildParams
    {
    public:
        BuildParams() = default;
        BuildParams(BuildParams const&) = default;
        BuildParams(BuildParams&&) noexcept = default;

        BuildParams& operator=(BuildParams const&) = default;
        BuildParams& operator=(BuildParams&&) noexcept = default;

        template <typename Tag>
        std::optional<AnySharedSignal<typename Tag::type>> get() const
        {
            auto r = params_.find(typeid(Tag));
            if (r == params_.end())
                return std::nullopt;

            return std::any_cast<AnySharedSignal<typename Tag::type>>(r->second);
        }

        template <typename Tag>
        void set(AnySharedSignal<typename Tag::type> value)
        {
            params_.insert_or_assign(typeid(Tag), std::move(value));
        }

    private:
        std::unordered_map<std::type_index, std::any> params_;
    };

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
            return AnyWidget(std::move(*func_));
        }

        template <typename T>
        auto operator|(WidgetModifier<T> modifier) &&
        {
            return std::invoke(
                    std::move(modifier),
                    std::move(*this)
                    );
        }

    private:
        btl::CloneOnCopy<TFunc> func_;
    };

    template <typename TFunc, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyBuilder, TFunc, BuildParams>
        >
    >
    auto makeWidget(TFunc&& func)
    {
        return Widget<std::decay_t<TFunc>>(
                WidgetBuildTag{},
                std::forward<TFunc>(func)
                );
    }

    template <typename TFunc, typename T, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyBuilder, TFunc, BuildParams, T>
        >
    >
    auto makeWidget(TFunc&& func, T&& t)
    {
        return makeWidget([
                func=btl::cloneOnCopy(std::forward<TFunc>(func)),
                t=btl::cloneOnCopy(std::forward<T>(t))
                ](BuildParams params) mutable
                {
                    return std::invoke(
                            std::move(*func),
                            std::move(params),
                            std::move(*t)
                            );
                });
    }

    template <typename TFunc, typename T, typename U, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyBuilder, TFunc, BuildParams, T&&, U&&>
        >
    >
    auto makeWidget(TFunc&& func, T&& t, U&& u)
    {
        return makeWidget([
                func=btl::cloneOnCopy(std::forward<TFunc>(func)),
                t=btl::cloneOnCopy(std::forward<T>(t)),
                u=btl::cloneOnCopy(std::forward<U>(u))
                ](BuildParams params) mutable
                {
                    return std::invoke(
                            std::move(*func),
                            std::move(params),
                            std::move(*t),
                            std::move(*u)
                            );
                });
    }

    inline auto makeWidget()
    {
        return makeWidget([](auto&&)
            {
                return makeBuilder();
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
            return AnyWidgetModifier(std::move(*func_));
        }

    private:
        btl::CloneOnCopy<TFunc> func_;
    };

    template <typename TFunc, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyWidget, TFunc, AnyWidget>
        >
    >
    auto makeWidgetModifier(TFunc&& func)
    {
        return WidgetModifier<std::decay_t<TFunc>>(
                WidgetModifierBuildTag{},
                std::forward<TFunc>(func)
                );
    }

    template <typename TFunc, typename T, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyWidget, TFunc, AnyWidget, T>
        >
    >
    auto makeWidgetModifier(TFunc&& func, T&& t)
    {
        return makeWidgetModifier(
                [
                func=std::forward<TFunc>(func),
                t=btl::cloneOnCopy(std::forward<T>(t))
                ]
                (auto widget) mutable
                {
                    return std::invoke(
                            std::move(func),
                            std::move(widget),
                            std::move(*t)
                            );

                });
    }

    template <typename T>
    auto makeWidgetModifier(BuilderModifier<T> modifier)
    {
        return makeWidgetModifier([](auto widget, auto modifier)
                {
                    return makeWidget([
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
    auto makeWidgetModifier(InstanceModifier<T> modifier)
    {
        return makeWidgetModifier([](auto widget, auto modifier)
                {
                    return makeWidget([](BuildParams params, auto widget, auto modifier)
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
    auto operator|(Widget<T>&& widget, InstanceModifier<U> modifier)
    {
        return std::move(widget)
            | makeWidgetModifier(std::move(modifier))
            ;
    }


    template <typename T>
    auto setParams(Signal<T, BuildParams> params)
    {
        return makeWidgetModifier([](auto widget, auto params)
        {
            return std::invoke(
                    std::move(widget),
                    std::move(params)
                    );
        },
        std::move(params)
        );
    }

    template <typename TFunc, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyWidget, TFunc, AnyWidget, BuildParams const&>
        >
    >
    auto withParams(TFunc&& func)
    {
        return makeWidgetModifier([](auto widget, auto func)
            {
                return makeWidget(
                        [widget=btl::cloneOnCopy(std::move(widget)),
                        func=btl::cloneOnCopy(std::move(func))]
                        (BuildParams params) mutable -> AnyBuilder
                        {
                            return std::invoke(
                                    std::invoke(
                                        std::move(*func),
                                        std::move(*widget),
                                        params
                                        ),
                                    std::move(params)
                                    );
                        });
            },
            std::forward<TFunc>(func)
            );
    }

    template <typename TFunc, typename = std::enable_if_t<
        std::is_invocable_r_v<BuildParams, TFunc, BuildParams>
        >
    >
    auto modifyParams(TFunc&& func)
    {
        return makeWidgetModifier([](auto widget, auto func)
            {
                return makeWidget(
                        [widget=btl::cloneOnCopy(std::move(widget)),
                        func=btl::cloneOnCopy(std::move(func))]
                        (auto params) mutable -> AnyBuilder
                        {
                            return std::invoke(
                                    std::move(*widget),
                                    std::invoke(
                                        std::move(*func),
                                        std::move(params)
                                        )
                                    );

                        });
            },
            std::forward<TFunc>(func)
            );
    }
} // namespace reactive::widget

