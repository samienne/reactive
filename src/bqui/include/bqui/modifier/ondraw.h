#pragma once

#include "instancemodifier.h"
#include "widgetmodifier.h"

#include "bqui/provider/provideanimation.h"

#include <avg/animationoptions.h>
#include <avg/rendertree.h>

#include <btl/cloneoncopy.h>

#include <type_traits>

namespace bqui::modifier
{

namespace detail
{
    template <bool reverse, typename TFunc, typename T, typename... Ts>
    AnyWidgetModifier onDrawCustom(TFunc&& f,
            bq::signal::Signal<T, std::optional<avg::AnimationOptions>> animation,
            Ts&&... ts)
    {
        return makeWidgetModifier(makeInstanceModifier([f=std::forward<TFunc>(f)]
            (widget::Instance instance, auto&& animation, auto&&... ts) mutable
            {
                auto shape = avg::makeShapeNode(
                        instance.getObb(),
                        std::forward<decltype(animation)>(animation),
                        f,
                        std::forward<decltype(ts)>(ts)...
                        );

                auto container = std::make_shared<avg::ContainerNode>(avg::Obb());

                if constexpr (reverse)
                {
                    container->addChild(std::move(shape));
                    container->addChild(instance.getRenderTree().getRoot());
                }
                else
                {
                    container->addChild(instance.getRenderTree().getRoot());
                    container->addChild(std::move(shape));
                }

                return std::move(instance)
                    .setRenderTree(avg::RenderTree(std::move(container)))
                    ;
            },
            std::move(animation),
            std::forward<Ts>(ts)...
            ));
    }
} // namespace detail

template <typename TFunc, typename... Ts,
         typename = std::enable_if_t<
         btl::isInvocableRV<
            avg::Drawing,
            TFunc,
            avg::DrawContext const&,
            avg::Vector2f,
            btl::ParamPackMapT<
                avg::AnimatedTypeT,
                bq::signal::UnpackSignalResultT<
                    bq::signal::DecaySignalResultT<
                        bq::signal::SignalTypeT<
                            provider::ParamProviderTypeT<std::decay_t<Ts>>
                        >
                    >
                >
            >...
        >
    >
>
AnyWidgetModifier onDraw(TFunc&& func, Ts&&... ts)
{
    return detail::makeWidgetModifierUnchecked(
            [](auto element, auto&& animation, auto&& func, auto&&... ts)
        {
            return std::move(element)
                | detail::onDrawCustom<false>(
                        std::forward<decltype(func)>(func),
                        std::forward<decltype(animation)>(animation),
                        std::forward<decltype(ts)>(ts)...
                        );
        },
        provider::provideAnimation(),
        std::forward<TFunc>(func),
        std::forward<Ts>(ts)...
        );
}

template <typename TFunc, typename... Ts,
         typename = std::enable_if_t<
         btl::isInvocableRV<
            avg::Drawing,
            TFunc,
            avg::DrawContext const&,
            avg::Vector2f,
            btl::ParamPackMapT<
                avg::AnimatedTypeT,
                bq::signal::UnpackSignalResultT<
                    bq::signal::DecaySignalResultT<
                        bq::signal::SignalTypeT<
                            provider::ParamProviderTypeT<std::decay_t<Ts>>
                        >
                    >
                >
            >...
        >
    >
>
AnyWidgetModifier onDrawBehind(TFunc&& func, Ts&&... ts)
{
    return detail::makeWidgetModifierUnchecked(
        [](auto element, auto&& animation, auto&& func, auto&&... ts)
        {
            return std::move(element)
                | detail::onDrawCustom<true>(
                        std::forward<decltype(func)>(func),
                        std::forward<decltype(animation)>(animation),
                        std::forward<decltype(ts)>(ts)...
                        );
        },
        provider::provideAnimation(),
        std::forward<TFunc>(func),
        std::forward<Ts>(ts)...
        );
}

}

