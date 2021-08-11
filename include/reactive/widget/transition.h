#pragma once

#include "avg/rendertree.h"
#include "transform.h"
#include "bindrendertree.h"
#include "setrendertree.h"
#include "widgettransformer.h"

namespace reactive::widget
{

auto transition()
{
    avg::UniqueId transitionId;
    avg::UniqueId containerId;

    return makeWidgetTransformer()
        .compose(grabRenderTree())
        .bind([transitionId, containerId](auto renderTree)
        {
            auto newTree = signal::map([transitionId, containerId]
            (avg::RenderTree const& renderTree)
            {
                auto activeContainer = std::make_shared<avg::ContainerNode>(
                        containerId,
                        avg::Obb(),
                        avg::TransitionOptions {}
                        );

                activeContainer->addChild(renderTree.getRoot());

                auto transitionedContainer = std::make_shared<avg::ContainerNode>(
                        containerId,
                        avg::Transform().translate(-200, 0) * avg::Obb(),
                        avg::TransitionOptions {}
                        );

                transitionedContainer->addChild(renderTree.getRoot());

                auto transition = std::make_shared<avg::TransitionNode>(
                        transitionId,
                        avg::Obb(),
                        avg::TransitionOptions {},
                        true,
                        std::move(activeContainer),
                        std::move(transitionedContainer)
                        );

                return avg::RenderTree(std::move(transition));
            },
            std::move(renderTree)
            );

            return setRenderTree(std::move(newTree));
        });
}

} // namespace reactive::widget

