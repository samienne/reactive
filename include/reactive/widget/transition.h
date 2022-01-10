#pragma once

#include "transform.h"
#include "setrendertree.h"
#include "instancemodifier.h"

#include <avg/rendertree.h>

#include <btl/cloneoncopy.h>

namespace reactive::widget
{

template <typename T>
struct Transition
{
    InstanceModifier<T> active;
    InstanceModifier<T> transitioned;
};

template <typename T>
auto makeTransition(InstanceModifier<T> active, InstanceModifier<T> transitioned)
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
        return makeInstanceModifier([offset](Instance instance)
            {
                auto container = std::make_shared<avg::ContainerNode>(
                        avg::Transform().translate(offset * instance.getSize()[0], 0)
                            * avg::Obb(instance.getSize())
                        );

                container->addChild(instance.getRenderTree().getRoot());

                return std::move(instance)
                    .setRenderTree(avg::RenderTree(std::move(container)));
            });
    };

    return makeTransition(makeTransformer(0.0f), makeTransformer(-1.0f));
}

template <typename T>
auto transition(Transition<T> transition)
{
    return makeSharedInstanceSignalModifier(
        [transition=std::move(transition)](auto widget) mutable
        {
            auto activeWidget = transition.active(widget.clone());
            auto transitionedWidget = transition.transitioned(widget.clone());

            auto newRenderTree = group(activeWidget.clone(),
                    transitionedWidget.clone())
                    .map([=](auto const& activeRenderTree, auto const& transitionedRenderTree)
                    {
                        auto transition = std::make_shared<avg::TransitionNode>(
                                avg::Obb(),
                                true,
                                activeRenderTree.getRenderTree().getRoot(),
                                transitionedRenderTree.getRenderTree().getRoot()
                                );

                        return avg::RenderTree(std::move(transition));
                    });

            return std::move(widget)
                | setRenderTree(std::move(newRenderTree))
                ;
        });
}

} // namespace reactive::widget

