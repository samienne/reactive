#pragma once

#include "instancemodifier.h"
#include "elementmodifier.h"

#include "bqui/widget/builder.h"

#include <btl/cloneoncopy.h>
#include <btl/all.h>
#include <btl/not.h>
#include <btl/bindarguments.h>

#include <functional>

namespace bqui::modifier
{
    struct BuilderModifierBuildTag {};

    template <typename TFunc>
    class BuilderModifier
    {
    public:
        BuilderModifier(BuilderModifierBuildTag const&, TFunc func) :
            func_(std::move(func))
        {
        }

        BuilderModifier(BuilderModifier const&) = default;
        BuilderModifier(BuilderModifier&&) noexcept = default;

        BuilderModifier& operator=(BuilderModifier const&) = default;
        BuilderModifier& operator=(BuilderModifier&&) noexcept = default;

        template <typename T, typename U>
        auto operator()(widget::Builder<T, U> builder) &&
        {
            return std::invoke(std::move(*func_), std::move(builder));
        }

    private:
        btl::CloneOnCopy<std::decay_t<TFunc>> func_;
    };

    extern template class BQUI_EXPORT_TEMPLATE
        BuilderModifier<std::function<widget::AnyBuilder(widget::AnyBuilder)>>;

    using AnyBuilderModifier = BuilderModifier<std::function<widget::AnyBuilder(
            widget::AnyBuilder)>>;

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

        struct MakeBuilderModifierUnchecked1
        {
            template <typename T, typename TFunc, typename... Ts>
            auto operator()(T&& builder, TFunc&& func, Ts&&... ts) const
            {
                auto params = builder.getBuildParams();

                return std::invoke(
                        std::forward<decltype(func)>(func),
                        std::forward<T>(builder),
                        provider::invokeParamProvider(
                            std::forward<decltype(ts)>(ts),
                            params
                            )...
                        );
            }
        };

        template <typename TFunc, typename... Ts>
        auto makeBuilderModifierUnchecked(TFunc&& func, Ts&&... ts)
        {
            return makeBuilderModifierUnchecked(btl::bindArguments(
                        detail::MakeBuilderModifierUnchecked1(),
                        std::forward<TFunc>(func),
                        std::forward<Ts>(ts)...
                        )
                    );
        }
    } // namespace detail

    template <typename TFunc, typename... Ts,
        typename = std::enable_if_t<
            std::is_invocable_r_v<
                widget::AnyBuilder, TFunc, widget::AnyBuilder,
                    provider::ParamProviderTypeT<Ts>...
                >
        >
    >
    auto makeBuilderModifier(TFunc&& func, Ts&&... ts)
    {
        return detail::makeBuilderModifierUnchecked(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }

    namespace detail
    {
        struct MakeBuilderModifierFromElement1
        {
            template <typename T, typename U>
            auto operator()(T&& builder, U&& f) const
            {
                auto sizeHint = builder.getSizeHint();
                auto gravity = builder.getGravity();

                return widget::makeBuilder([builder=std::forward<T>(builder),
                    modifier=std::forward<U>(f)]
                    (BuildParams const& /*params*/, auto size)
                    {
                        auto element = builder.clone()
                            (std::move(size))
                            | modifier;

                        return element;
                    },
                    std::move(sizeHint),
                    {},
                    std::move(gravity)
                    );
            }
        };
    } // namespace detail

    template <typename TFunc>
    auto makeBuilderModifier(ElementModifier<TFunc> f)
    //-> BuilderModifier
    {
        return detail::makeBuilderModifierUnchecked(
                detail::MakeBuilderModifierFromElement1(),
                std::move(f)
                );
    }

    template <typename TFunc>
    auto makeBuilderModifier(InstanceModifier<TFunc> f)
    //-> BuilderModifier
    {
        return makeBuilderModifier(makeElementModifier(std::move(f)));
    }

    template <typename T, typename... Ts>
    auto operator|(widget::Builder<Ts...> builder, BuilderModifier<T> f)
    -> decltype(
            std::move(f)(std::move(builder))
            )
        // -> Builder
    {
        return std::move(f)(std::move(builder));
    }

    template <typename T, typename... Ts>
    auto operator|(widget::Builder<Ts...> builder, InstanceModifier<T> f)
    {
        return std::invoke(
                makeBuilderModifier(std::move(f)),
                std::move(builder)
                );
    }


    template <typename TBuilderModifier, typename... Ts>
    auto operator|(widget::Builder<Ts...> builder, TBuilderModifier&& f)
    -> decltype(std::move(builder)
            .map(std::forward<TBuilderModifier>(f))
            )
        //-> Builder
    {
        return std::move(builder)
            .map(std::forward<TBuilderModifier>(f));
    }
}

