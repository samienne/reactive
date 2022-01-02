#pragma once

#include "transform.h"
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
        return makeWidgetModifier([offset](Widget widget)
            {
                auto container = std::make_shared<avg::ContainerNode>(
                        avg::Transform().translate(offset * widget.getSize()[0], 0)
                            * avg::Obb(widget.getSize())
                        );

                container->addChild(widget.getRenderTree().getRoot());

                return std::move(widget)
                    .setRenderTree(avg::RenderTree(std::move(container)));
            });
    };

    return makeTransition(makeTransformer(0.0f), makeTransformer(-1.0f));
}

template <typename T>
auto transition(Transition<T> transition)
{
    return makeWidgetSignalModifier([transition=std::move(transition)](auto widget) mutable
        {
            auto w = signal::share(std::move(widget));

            auto [activeWidget, p1] = transition.active(w.clone());
            auto [transitionedWidget, p2] = transition.transitioned(w.clone());

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

            return std::move(w)
                | setRenderTree(std::move(newRenderTree))
                ;
        });
}

} // namespace reactive::widget

