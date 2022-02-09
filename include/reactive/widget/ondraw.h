#pragma once

#include "instancemodifier.h"
#include "setanimation.h"

#include <avg/animationoptions.h>
#include <avg/rendertree.h>

#include <btl/cloneoncopy.h>

#include <type_traits>

namespace reactive::widget
{

namespace detail
{
    template <bool reverse, typename TFunc, typename T, typename... Ts>
    auto onDrawCustom(TFunc&& f,
            Signal<T, std::optional<avg::AnimationOptions>> animation,
            Ts&&... ts)
    {
        return makeInstanceModifier([f=std::forward<TFunc>(f)]
            (Instance instance, auto&& animation, auto&&... ts) mutable
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
            );
    }
} // namespace detail

template <typename TFunc, typename... Ts,
         typename = std::enable_if_t<
            std::is_invocable_r_v<
                avg::Drawing,
                TFunc,
                avg::DrawContext const&,
                avg::Vector2f,
                avg::AnimatedTypeT<std::decay_t<signal::SignalType<std::decay_t<Ts>>>>...
            >
        >
    >
auto onDraw(TFunc&& func, Ts&&... ts)
{
    return makeElementModifier([](auto element, auto&& func, auto&&... ts)
        {
            auto animation = element.getParams()
                .template valueOrDefault<AnimationTag>()
                ;

            return std::move(element)
                | detail::onDrawCustom<false>(
                        std::forward<decltype(func)>(func),
                        std::move(animation),
                        std::forward<decltype(ts)>(ts)...
                        );
        },
        std::forward<TFunc>(func),
        std::forward<Ts>(ts)...
        );
}

template <typename TFunc, typename... Ts,
         typename = std::enable_if_t<
            std::is_invocable_r_v<
                avg::Drawing,
                TFunc,
                avg::DrawContext const&,
                avg::Vector2f,
                avg::AnimatedTypeT<std::decay_t<signal::SignalType<std::decay_t<Ts>>>>...
            >
        >
    >
auto onDrawBehind(TFunc&& func, Ts&&... ts)
{
    return makeElementModifier([](auto element, auto&& func, auto&&... ts)
        {
            auto animation = element.getParams()
                .template valueOrDefault<AnimationTag>()
                ;

            return std::move(element)
                | detail::onDrawCustom<true>(
                        std::forward<decltype(func)>(func),
                        std::move(animation),
                        std::forward<decltype(ts)>(ts)...
                        );
        },
        std::forward<TFunc>(func),
        std::forward<Ts>(ts)...
        );
}

} // namespace reactive::widget

