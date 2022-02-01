#pragma once

#include "instancemodifier.h"
#include "elementmodifier.h"
#include "builder.h"

#include <btl/cloneoncopy.h>
#include <btl/all.h>
#include <btl/not.h>
#include <btl/bindarguments.h>

#include <functional>

namespace reactive::widget
{
    struct BuilderModifierBuildTag {};

    template <typename TFunc>
    struct BuilderModifier
    {
        BuilderModifier(BuilderModifierBuildTag const&, TFunc func) :
            func_(std::move(func))
        {
        }

        BuilderModifier(BuilderModifier const&) = default;
        BuilderModifier(BuilderModifier&&) noexcept = default;

        BuilderModifier& operator=(BuilderModifier const&) = default;
        BuilderModifier& operator=(BuilderModifier&&) noexcept = default;

        template <typename T, typename U>
        auto operator()(Builder<T, U> builder) &&
        {
            return std::invoke(std::move(*func_), std::move(builder));
        }

        btl::CloneOnCopy<std::decay_t<TFunc>> func_;
    };

    using AnyBuilderModifier = BuilderModifier<std::function<AnyBuilder(AnyBuilder)>>;

    template <typename T>
    struct IsBuilderModifier : std::false_type {};

    template <typename T>
    struct IsBuilderModifier<BuilderModifier<T>> : std::true_type {};

    namespace detail
    {
        template <typename TFunc>
        auto makeBuilderModifierUnchecked(TFunc&& func)
        {
            return BuilderModifier<std::decay_t<TFunc>>(
                    BuilderModifierBuildTag{},
                    std::forward<TFunc>(func)
                    );
        }

        template <typename... TParamTags, typename TFunc, typename... Ts>
        auto makeBuilderModifierUnchecked(TFunc&& func, Ts&&... ts)
        {
            return makeBuilderModifierUnchecked(btl::bindArguments(
                        [](auto builder, auto&& func, auto&&... ts)
                        {
                            if constexpr (sizeof...(TParamTags) > 0)
                            {
                                auto params = builder.getBuildParams();

                                return std::invoke(
                                        std::forward<decltype(func)>(func),
                                        std::move(builder),
                                        params.template valueOrDefault<TParamTags>()...,
                                        std::forward<decltype(ts)>(ts)...
                                        );
                            }
                            else
                            {
                                return std::invoke(
                                        std::forward<decltype(func)>(func),
                                        std::move(builder),
                                        std::forward<decltype(ts)>(ts)...
                                        );
                            }
                        },
                        std::forward<TFunc>(func),
                        std::forward<Ts>(ts)...
                        )
                    );
        }
    } // namespace detail

    template <typename... TParamTags, typename TFunc, typename... Ts,
        typename = std::enable_if_t<
            std::is_invocable_r_v<
                AnyBuilder, TFunc, AnyBuilder,
                    AnySharedSignal<typename TParamTags::type>..., Ts&&...
                >
        >
    >
    auto makeBuilderModifier(TFunc&& func, Ts&&... ts)
    {
        return detail::makeBuilderModifierUnchecked<TParamTags...>(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }

    template <typename TFunc>
    auto makeBuilderModifier(widget::ElementModifier<TFunc> f)
    //-> BuilderModifier
    {
        return detail::makeBuilderModifierUnchecked([](auto builder, auto f)
            {
                return std::move(builder)
                    .map(std::move(f))
                    ;
            },
            std::move(f)
            );
    }

    template <typename TFunc>
    auto makeBuilderModifier(widget::InstanceModifier<TFunc> f)
    //-> BuilderModifier
    {
        return makeBuilderModifier(makeElementModifier(std::move(f)));
    }


    template <typename TFunc>
    auto makeBuilderPreModifier(widget::ElementModifier<TFunc> f)
    //-> BuilderModifier
    {
        return detail::makeBuilderModifierUnchecked([](auto builder, auto f)
            {
                return std::move(builder)
                    .preMap(std::move(f))
                    ;
            },
            std::move(f)
            );
    }

    template <typename TFunc>
    auto makeBuilderPreModifier(widget::InstanceModifier<TFunc> f)
    {
        return makeBuilderPreModifier(makeElementModifier(std::move(f)));
    }

    template <typename T, typename... Ts>
    auto operator|(Builder<Ts...> builder, BuilderModifier<T> f)
    -> decltype(
            std::move(f)(std::move(builder))
            )
        // -> Builder
    {
        return std::move(f)(std::move(builder));
    }

    template <typename T, typename... Ts>
    auto operator|(Builder<Ts...> builder, InstanceModifier<T> f)
    {
        return std::invoke(
                makeBuilderModifier(std::move(f)),
                std::move(builder)
                );
    }


    template <typename TBuilderModifier, typename... Ts>
    auto operator|(Builder<Ts...> builder, TBuilderModifier&& f)
    -> decltype(std::move(builder)
            .map(std::forward<TBuilderModifier>(f))
            )
        //-> Builder
    {
        return std::move(builder)
            .map(std::forward<TBuilderModifier>(f));
    }
} // namespace reactive::widget

