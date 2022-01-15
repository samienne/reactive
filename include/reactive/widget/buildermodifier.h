#pragma once

#include "instancemodifier.h"
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

        template <typename TFunc, typename... Ts>
        auto makeBuilderModifierUnchecked(TFunc&& func, Ts&&... ts)
        {
            return makeBuilderModifierUnchecked(btl::bindArguments(
                        [](auto builder, auto&& func, auto&&... ts)
                        {
                            return std::invoke(
                                    std::forward<decltype(func)>(func),
                                    std::move(builder),
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
        std::is_invocable_r_v<AnyBuilder, TFunc, AnyBuilder, Ts&&...>
        >
    >
    auto makeBuilderModifier(TFunc&& func, Ts&&... ts)
    {
        return detail::makeBuilderModifierUnchecked(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }

    template <typename TFunc>
    auto makeBuilderModifier(widget::InstanceModifier<TFunc> f)
    //-> BuilderModifier
    {
        return makeBuilderModifier([](auto builder, auto f)
            {
                return std::move(builder)
                    .map(std::move(f))
                    ;
            },
            std::move(f)
            );
    }

    template <typename TFunc>
    auto makeBuilderPreModifier(widget::InstanceModifier<TFunc> f)
    //-> BuilderModifier
    {
        return makeBuilderModifier([](auto builder, auto f)
            {
                return std::move(builder)
                    .preMap(std::move(f))
                    ;
            },
            std::move(f)
            );
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

    template <typename TInstanceModifier, typename... Ts>
    auto operator|(Builder<Ts...> builder, TInstanceModifier&& f)
    -> decltype(std::move(builder)
            .map(std::forward<TInstanceModifier>(f))
            )
        //-> Builder
    {
        return std::move(builder)
            .map(std::forward<TInstanceModifier>(f));
    }
} // namespace reactive::widget

