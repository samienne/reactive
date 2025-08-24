#pragma once

#include "builder.h"

#include "bqui/modifier/elementmodifier.h"

#include "bqui/provider/providebuildparams.h"

#include "bqui/buildparams.h"

#include <bq/signal/signal.h>

#include <avg/vector.h>

#include <btl/pushback.h>
#include <btl/bindarguments.h>

#include <type_traits>
#include <utility>

namespace bqui::widget
{
    struct WidgetBuildTag {};

    template <typename TFunc>
    class Widget;

    using AnyWidget = Widget<std::function<AnyBuilder(BuildParams)>>;

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

        auto clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<TFunc> func_;
    };

    extern template class BQUI_EXPORT_TEMPLATE
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
                                provider::invokeParamProvider(
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
                                    provider::invokeParamProvider(
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
        std::is_invocable_r_v<AnyWidget, TFunc, provider::ParamProviderTypeT<Ts>...>
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
        provider::ParamProviderTypeT<Ts>...>
        >>
    auto makeWidgetWithSize(TFunc&& func, Ts&&... ts)
    {
        return makeWidget()
            | modifier::detail::makeElementModifierUnchecked(
                    detail::MakeWidgetWithSize1(),
                    provider::provideBuildParams(),
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
}

