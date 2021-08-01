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
        avg::UniqueId id;
        avg::UniqueId containerId;

        return makeWidgetTransformer()
            .compose(grabRenderTree(), bindObb())
            .values(std::forward<Ts>(ts)...)
            .bind([f=std::forward<TFunc>(f), id, containerId](auto renderTree, auto obb,
                        auto&&... ts)
            {
                auto newTree = signal::map([f, id, containerId]
                    (avg::RenderTree const& renderTree, avg::Obb const& obb, auto&&... ts)
                    {
                        auto shape = avg::makeShapeNode(
                                id, obb, avg::TransitionOptions {}, f, ts...
                                );

                        auto container = std::make_shared<avg::ContainerNode>(
                                containerId, avg::Obb(), avg::TransitionOptions {});

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

