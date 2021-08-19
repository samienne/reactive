#pragma once

#include "transform.h"
#include "bindrendertree.h"
#include "bindobb.h"
#include "setrendertree.h"
#include "widgettransformer.h"

#include <avg/rendertree.h>

#include <btl/cloneoncopy.h>

namespace reactive::widget
{

template <typename T>
struct Transition
{
    WidgetTransformer<T> active;
    WidgetTransformer<T> transitioned;
};

template <typename T>
auto makeTransition(WidgetTransformer<T> active, WidgetTransformer<T> transitioned)
{
    return Transition<T> {
        std::move(active),
        std::move(transitioned)
    };
}

inline auto transitionLeft()
{
    auto makeTransformer = [](float offset)
    {
        return makeWidgetTransformer()
            .compose(bindObb(), grabRenderTree())
            .bind([=](auto obb, auto renderTree)
            {
                auto newRenderTree = group(std::move(renderTree), std::move(obb))
                        .map([=](auto renderTree, auto obb)
                        {
                            auto container = std::make_shared<avg::ContainerNode>(
                                    avg::Transform().translate(offset * obb.getSize()[0], 0)
                                        * avg::Obb(obb.getSize())
                                    );

                            container->addChild(renderTree.getRoot());

                            return avg::RenderTree(std::move(container));
                        });


                return setRenderTree(std::move(newRenderTree));
            });
    };

    return makeTransition(makeTransformer(0.0f), makeTransformer(-1.0f));
}

template <typename T>
auto transition(Transition<T> transition)
{
    return makeWidgetTransformer([=, transition=std::move(transition)]
        (auto widget) mutable
        {
            auto renderTree = share(widget.getRenderTree());

            auto w = std::move(widget).setRenderTree(std::move(renderTree));

            auto [activeWidget, p1] = transition.active(w.clone());
            auto [transitionedWidget, p2] = transition.transitioned(w.clone());

            auto newRenderTree = group(activeWidget.getRenderTree().clone(),
                    transitionedWidget.getRenderTree().clone())
                    .map([=](auto const& activeRenderTree, auto transitionedRenderTree)
                    {
                        auto transition = std::make_shared<avg::TransitionNode>(
                                avg::Obb(),
                                true,
                                activeRenderTree.getRoot(),
                                transitionedRenderTree.getRoot()
                                );

                        return avg::RenderTree(std::move(transition));
                    });

            //return setRenderTree(std::move(newRenderTree));

            return std::make_pair(
                    std::move(w).setRenderTree(std::move(newRenderTree)),
                    btl::cloneOnCopy(std::make_tuple())
                    );
        });

    /*
    return makeWidgetTransformer()
        .compose(grabRenderTree())
        .bind([transitionId, containerId](auto renderTree)
        {
            auto newTree = signal::map([transitionId, containerId]
            (avg::RenderTree const& renderTree)
            {
                auto activeContainer = std::make_shared<avg::ContainerNode>(
                        containerId,
                        avg::Obb()
                        );

                activeContainer->addChild(renderTree.getRoot());

                auto transitionedContainer = std::make_shared<avg::ContainerNode>(
                        containerId,
                        avg::Transform().translate(-200, 0) * avg::Obb()
                        );

                transitionedContainer->addChild(renderTree.getRoot());

                auto transition = std::make_shared<avg::TransitionNode>(
                        transitionId,
                        avg::Obb(),
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
        */
}

} // namespace reactive::widget

