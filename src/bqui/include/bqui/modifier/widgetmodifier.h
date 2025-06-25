#pragma once

#include "buildermodifier.h"

#include "bqui/widget/widget.h"

#include <functional>

namespace bqui::modifier
{
    template <typename TFunc>
    class WidgetModifier;

    using AnyWidgetModifier = WidgetModifier<std::function<widget::AnyWidget(
            widget::AnyWidget)>>;

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
        auto operator()(widget::Widget<T> widget) &&
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

    extern template class BQUI_EXPORT_TEMPLATE
        WidgetModifier<std::function<widget::AnyWidget(widget::AnyWidget)>>;

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

        struct MakeWidgetModifierUnchecked1
        {
            template <typename T, typename TFunc, typename... Ts>
            auto operator()(BuildParams const& params, T&& widget,
                    TFunc&& func, Ts&&... ts) const
            {
                return std::invoke(
                        std::forward<TFunc>(func),
                        std::forward<T>(widget),
                        std::forward<Ts>(ts)...
                        )(params);
            }
        };

        struct MakeWidgetModifierUnchecked2
        {
            template <typename TWidget, typename TFunc, typename... Ts>
            auto operator()(TWidget&& widget, TFunc&& func, Ts&&... ts) const
            {
                return widget::detail::makeWidgetUncheckedWithParams(
                        MakeWidgetModifierUnchecked1(),
                        std::forward<TWidget>(widget),
                        std::forward<TFunc>(func),
                        std::forward<Ts>(ts)...
                        );
            }
        };

        template <typename TFunc, typename... Ts>
        auto makeWidgetModifierUnchecked(TFunc&& func, Ts&&... ts)
        {
            return makeWidgetModifierUnchecked(btl::bindArguments(
                        MakeWidgetModifierUnchecked2(),
                        std::forward<TFunc>(func),
                        std::forward<Ts>(ts)...
                        )
                    );
        }
    } // namespace detail

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<widget::AnyWidget, TFunc, widget::EmptyWidget,
            provider::ParamProviderTypeT<Ts>...>
        >
    >
    auto makeWidgetModifier(TFunc&& func, Ts&&... ts)
    {
        return detail::makeWidgetModifierUnchecked(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }

    namespace detail
    {
        struct MakeWidgetModifier1
        {
            template <typename T, typename U>
            auto operator()(BuildParams params, T&& widget, U&& modifier) const
            {
                return std::forward<T>(widget)(std::move(params))
                    | std::forward<U>(modifier)
                    ;
            }
        };

        struct MakeWidgetModifier2
        {
            template <typename T, typename U>
            auto operator()(T&& widget, U&& modifier) const
            {
                return widget::detail::makeWidgetUncheckedWithParams(
                        MakeWidgetModifier1(),
                        std::forward<T>(widget),
                        std::forward<U>(modifier)
                        );
            }
        };
    } // namespace detail

    template <typename T>
    auto makeWidgetModifier(BuilderModifier<T> modifier)
    {
        return detail::makeWidgetModifierUnchecked(
                detail::MakeWidgetModifier2(),
                std::move(modifier)
                );
    }

    namespace detail
    {
        struct MakeWidgetModifier3
        {
            template <typename T, typename U>
            auto operator()(BuildParams params, T&& widget, U&& modifier) const
            {
                return std::forward<T>(widget)(std::move(params))
                    | makeBuilderModifier(std::forward<U>(modifier))
                    ;
            }
        };

        struct MakeWidgetModifier4
        {
            template <typename T, typename U>
            auto operator()(T&& widget, U&& modifier) const
            {
                return widget::detail::makeWidgetUncheckedWithParams(
                        detail::MakeWidgetModifier3(),
                        std::forward<T>(widget),
                        std::forward<U>(modifier)
                        );
            }
        };
    } // namespace detail

    template <typename T>
    auto makeWidgetModifier(ElementModifier<T> modifier)
    {
        return detail::makeWidgetModifierUnchecked(
                detail::MakeWidgetModifier4(),
                std::move(modifier)
                );
    }

    namespace detail
    {
        struct MakeWidgetModifier5
        {
            template <typename T, typename U>
            auto operator()(BuildParams params, T&& widget, U&& modifier) const
            {
                return std::forward<T>(widget)(std::move(params))
                    | makeBuilderModifier(std::forward<U>(modifier))
                    ;
            }
        };

        struct MakeWidgetModifier6
        {
            template <typename T, typename U>
            auto operator()(T&& widget, U&& modifier)
            {
                return widget::detail::makeWidgetUncheckedWithParams(
                        MakeWidgetModifier5(),
                        std::forward<T>(widget),
                        std::forward<U>(modifier));
            }
        };
    } // namespace detail

    template <typename T>
    auto makeWidgetModifier(InstanceModifier<T> modifier)
    {
        return makeWidgetModifier(
                detail::MakeWidgetModifier6(),
                std::move(modifier)
                );
    }

    template <typename T, typename U>
    auto operator|(widget::Widget<T>&& widget, BuilderModifier<U> modifier)
    {
        return std::move(widget)
            | makeWidgetModifier(std::move(modifier))
            ;
    }

    template <typename T, typename U>
    auto operator|(widget::Widget<T>&& widget, ElementModifier<U> modifier)
    {
        return std::move(widget)
            | makeWidgetModifier(std::move(modifier))
            ;
    }

    template <typename T, typename U>
    auto operator|(widget::Widget<T>&& widget, InstanceModifier<U> modifier)
    {
        return std::move(widget)
            | makeWidgetModifier(std::move(modifier))
            ;
    }

    namespace detail
    {
        struct MakeWidgetModifierWithSize1
        {
            template <typename U, typename TFunc, typename V, typename... Ts>
            auto operator()(V&& size, U&& builder, TFunc&& func, Ts&&... ts) const
            {
                return std::invoke(
                        std::forward<decltype(func)>(func),
                        makeWidgetFromBuilder(std::forward<U>(builder)),
                        std::forward<decltype(size)>(size),
                        std::forward<decltype(ts)>(ts)...
                        );
            }
        };

        struct MakeWidgetModifierWithSize2
        {
            template <typename T, typename U>
            auto operator()(T&& builder, U&& sizeHint) const
            {
                return std::forward<T>(builder)
                    .setSizeHint(std::forward<U>(sizeHint));
            }
        };

        struct MakeWidgetModifierWithSize3
        {
            template <typename T, typename U, typename... Ts>
            auto operator()(T&& widget, BuildParams const& params, U&& func,
                    Ts&&... ts) const
            {
                auto builder = std::forward<T>(widget)(params);

                auto sizeHint = builder.getSizeHint().clone();

                return makeWidgetWithSize(
                    detail::MakeWidgetModifierWithSize1(),
                    std::move(builder),
                    std::forward<decltype(func)>(func),
                    std::forward<decltype(ts)>(ts)...
                    )
                    | makeWidgetModifier(makeBuilderModifier(
                                detail::MakeWidgetModifierWithSize2(),
                                std::move(sizeHint)))
                    ;
            }
        };
    } // namespace detail

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<widget::AnyWidget, TFunc, widget::AnyWidget,
            bq::signal::AnySignal<avg::Vector2f>,
            provider::ParamProviderTypeT<Ts>...>
        >>
    auto makeWidgetModifierWithSize(TFunc&& func, Ts&&... ts)
    {
        return makeWidgetModifier(
                detail::MakeWidgetModifierWithSize3(),
                provider::provideBuildParams(),
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }

    template <typename T, typename U>
    auto operator|(widget::Widget<T>&& widget, WidgetModifier<U> modifier)
    {
        return std::move(modifier)(std::move(widget));
    }
}

