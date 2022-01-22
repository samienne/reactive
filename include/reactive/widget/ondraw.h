#pragma once

#include "instancemodifier.h"

#include <avg/rendertree.h>

#include <btl/cloneoncopy.h>

#include <type_traits>

namespace reactive::widget
{

namespace detail
{
    template <bool reverse, typename TFunc, typename... Ts>
    auto onDrawCustom(TFunc&& f, Ts&&... ts)
    {
        return makeInstanceModifier([f=std::forward<TFunc>(f)]
            (Instance instance, auto&&... ts) mutable
            {
                auto shape = avg::makeShapeNode(
                        instance.getObb(),
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
            std::forward<Ts>(ts)...
            );
    }
} // namespace detail

// func(DrawContext, size, ...)
template <typename TFunc, typename... Ts,
         typename = std::enable_if_t<
            std::is_invocable_r_v<
                avg::Drawing,
                TFunc,
                avg::DrawContext const&,
                avg::Vector2f,
                signal::SignalType<std::decay_t<Ts>>...
            >
        >
    >
auto onDraw(TFunc&& func, Ts&&... ts)
{
    return detail::onDrawCustom<false>(
            std::forward<TFunc>(func),
            std::forward<decltype(ts)>(ts)...
            );
}

// func(DrawContext, size, ...)
template <typename TFunc, typename... Ts,
         typename = std::enable_if_t<
            std::is_invocable_r_v<
                avg::Drawing,
                TFunc,
                avg::DrawContext const&,
                avg::Vector2f,
                avg::AnimatedTypeT<signal::SignalType<std::decay_t<Ts>>>...
            >
        >
    >
auto onDrawBehind(TFunc&& func, Ts&&... ts)
{
    return detail::onDrawCustom<true>(
            std::forward<TFunc>(func),
            std::forward<decltype(ts)>(ts)...
            );
}

} // namespace reactive::widget

