#pragma once

#include "providebuildparams.h"
#include "buildparams.h"
#include "buildermodifier.h"

#include <bq/signal/signal.h>

#include <avg/vector.h>

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
            return std::move(modifier)(std::move(*this));
        }

        auto clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<TFunc> func_;
    };

    extern template class REACTIVE_EXPORT_TEMPLATE
        Widget<std::function<AnyBuilder(BuildParams)>>;

    namespace detail
    {
        template <typename TFunc>
        auto makeWidgetUncheckedWithParams(TFunc&& func)
        {
            return Widget<std::decay_t<TFunc>>(
                    WidgetBuildTag{},
                    std::forward<TFunc>(func)
                    );
        }

        template <typename TFunc, typename... Ts>
        auto makeWidgetUncheckedWithParams(TFunc&& func, Ts&&... ts)
        {
            return makeWidgetUncheckedWithParams(btl::bindArguments(
                        [](BuildParams const& params, auto&& func, auto&&... ts)
                        {
                            return std::invoke(
                                std::forward<decltype(func)>(func),
                                params,
                                invokeParamProvider(
                                    std::forward<decltype(ts)>(ts),
                                    params
                                    )...
                                );
                        },
                        std::forward<TFunc>(func),
                        std::forward<Ts>(ts)...
                        )
                    );
        }

        template <typename TFunc, typename... Ts>
        auto makeWidgetUnchecked(TFunc&& func, Ts&&... ts)
        {
            return makeWidgetUncheckedWithParams(btl::bindArguments(
                        [](BuildParams const& params, auto&& func, auto&&... ts)
                        {
                            return std::invoke(
                                    std::forward<decltype(func)>(func),
                                    invokeParamProvider(
                                        std::forward<decltype(ts)>(ts),
                                        params
                                        )...
                                    )(params);
                        },
                        std::forward<TFunc>(func),
                        std::forward<Ts>(ts)...
                        )
                    );
        }
    } // namespace detail

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyWidget, TFunc, ParamProviderTypeT<Ts>...>
        >>
    auto makeWidget(TFunc&& func, Ts&&... ts)
    {
        return detail::makeWidgetUnchecked(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }

    inline auto makeWidget()
    {
        return detail::makeWidgetUncheckedWithParams([](auto&& params)
            {
                return makeBuilder()
                    .setBuildParams(std::forward<decltype(params)>(params))
                    ;
            });
    }

    using EmptyWidget = std::decay_t<decltype(makeWidget())>;

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

    extern template class REACTIVE_EXPORT_TEMPLATE
        WidgetModifier<std::function<AnyWidget(AnyWidget)>>;

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
                return makeWidgetUncheckedWithParams(
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
        std::is_invocable_r_v<AnyWidget, TFunc, EmptyWidget, ParamProviderTypeT<Ts>...>
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
                return detail::makeWidgetUncheckedWithParams(
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
                return detail::makeWidgetUncheckedWithParams(
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
                return detail::makeWidgetUncheckedWithParams(
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

    namespace detail
    {
        struct MakeWidgetWithSize1
        {
            template <typename T, typename TFunc, typename... Ts>
            auto operator()(T&& element, BuildParams const& params, TFunc&& func,
                    Ts&&... ts) const
            {
                auto sharedElement = std::forward<T>(element).share();
                auto size = sharedElement.getSize();

                auto widget = std::invoke(
                        std::forward<TFunc>(func),
                        size.clone(),
                        std::forward<decltype(ts)>(ts)...
                        );

                return std::move(widget)(params)(size.clone());
            }
        };
    } // namespace detail

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyWidget, TFunc, bq::signal::AnySignal<avg::Vector2f>,
        ParamProviderTypeT<Ts>...>
        >>
    auto makeWidgetWithSize(TFunc&& func, Ts&&... ts)
    {
        return makeWidget()
            | detail::makeElementModifierUnchecked(
                    detail::MakeWidgetWithSize1(),
                    provideBuildParams(),
                    std::forward<TFunc>(func),
                    std::forward<Ts>(ts)...
                    );
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
            auto operator()(T&& widget, BuildParams const& params, U&& func, Ts&&... ts) const
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
        std::is_invocable_r_v<AnyWidget, TFunc, AnyWidget, bq::signal::AnySignal<avg::Vector2f>,
        ParamProviderTypeT<Ts>...>
        >>
    auto makeWidgetModifierWithSize(TFunc&& func, Ts&&... ts)
    {
        return makeWidgetModifier(
                detail::MakeWidgetModifierWithSize3(),
                provideBuildParams(),
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }

    namespace detail
    {
        struct MakeWidgetFromBuilder1
        {
            template <typename T>
            auto operator()(BuildParams const& params, T&& builder) const
            {
                return std::forward<T>(builder)
                    .setBuildParams(params);
            }
        };
    } // namespace detail

    template <typename T, typename U>
    AnyWidget makeWidgetFromBuilder(Builder<T, U> builder)
    {
        return detail::makeWidgetUncheckedWithParams(
                detail::MakeWidgetFromBuilder1(),
                std::move(builder)
                );
    }

    template <typename T>
    AnyWidget makeWidgetFromElement(Element<T> element)
    {
        return makeWidgetFromBuilder(
                makeBuilderFromElement(std::move(element)));
    }
} // namespace reactive::widget

