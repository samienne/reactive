#pragma once

#include "setrendertree.h"
#include "bindrendertree.h"
#include "bindobb.h"

#include <avg/rendertree.h>

#include <type_traits>

namespace reactive::widget
{

namespace detail
{
    template <bool reverse, typename TFunc, typename... Ts,
             typename = std::enable_if_t
            <
            std::is_invocable_r_v<avg::Drawing, TFunc, avg::DrawContext const&,
                avg::Vector2f, signal::SignalType<std::decay_t<Ts>>...>

            >
        >
    auto onDrawCustom(TFunc&& f, Ts&&... ts)
    {
        /*
        return makeWidgetTransformer()
            .compose(grabRenderTree(), bindObb())
            .values(std::forward<Ts>(ts)...)
            .bind([f=std::forward<TFunc>(f)](auto renderTree, auto obb,
                        auto&&... ts)
            {
                auto newTree = signal::map([f]
                    (avg::RenderTree const& renderTree, avg::Obb const& obb, auto&&... ts)
                    {
                        auto shape = avg::makeShapeNode(obb, f, ts...);

                        auto container = std::make_shared<avg::ContainerNode>(avg::Obb());

                        if (reverse)
                        {
                            container->addChild(std::move(shape));
                            container->addChild(renderTree.getRoot());
                        }
                        else
                        {
                            container->addChild(renderTree.getRoot());
                            container->addChild(std::move(shape));
                        }


                        return avg::RenderTree(std::move(container));
                    },
                    std::move(renderTree),
                    std::move(obb),
                    std::forward<decltype(ts)>(ts)...
                    );

                return setRenderTree(std::move(newTree));
            });
        */

        return makeWidgetModifier([f=std::forward<TFunc>(f)]
            (auto widget, auto&&... ts)
            {
                return signal::map([f]
                    (Widget widget, auto... ts)
                    {
                        auto shape = avg::makeShapeNode(widget.getObb(), f, ts...);

                        auto container = std::make_shared<avg::ContainerNode>(avg::Obb());

                        if (reverse)
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
template <typename TFunc>
auto onDrawCustom(TFunc&& func)
{
    return [func=std::forward<TFunc>(func)](auto&&... ts) mutable
    {
        return detail::onDrawCustom<false>(
                std::move(func),
                std::forward<decltype(ts)>(ts)...
                );
    };
}

// func(DrawContext, size, ...)
template <typename TFunc>
auto onDrawBehindCustom(TFunc&& func)
{
    return [func=std::forward<TFunc>(func)](auto&&... ts) mutable
    {
        return detail::onDrawCustom<true>(
                std::move(func),
                std::forward<decltype(ts)>(ts)...
                );
    };
}

} // namespace reactive::widget

