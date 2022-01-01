#pragma once

#include "setrendertree.h"
#include "bindrendertree.h"
#include "bindobb.h"

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
        return makeWidgetModifier([f=std::forward<TFunc>(f)]
            (auto widget, auto&&... ts) mutable
            {
                return signal::map([f]
                    (Widget widget, auto... ts)
                    {
                        auto shape = avg::makeShapeNode(widget.getObb(), f, ts...);

                        auto container = std::make_shared<avg::ContainerNode>(avg::Obb());

                        if constexpr (reverse)
                        {
                            container->addChild(std::move(shape));
                            container->addChild(widget.getRenderTree().getRoot());
                        }
                        else
                        {
                            container->addChild(widget.getRenderTree().getRoot());
                            container->addChild(std::move(shape));
                        }

                        return std::move(widget)
                            .setRenderTree(avg::RenderTree(std::move(container)))
                            ;
                    },
                    std::move(widget),
                    std::forward<decltype(ts)>(ts)...
                    );
            },
            std::forward<Ts>(ts)...
        );
    }

} // namespace

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
                signal::SignalType<std::decay_t<Ts>>...
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

