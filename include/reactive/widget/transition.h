#pragma once

#include "transform.h"
#include "setrendertree.h"
#include "widget.h"

#include <avg/rendertree.h>

#include <btl/cloneoncopy.h>

namespace reactive::widget
{

template <typename T>
struct Transition
{
    WidgetModifier<T> active;
    WidgetModifier<T> transitioned;
};

template <typename T>
auto makeTransition(WidgetModifier<T> active, WidgetModifier<T> transitioned)
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

    return makeTransition(
            makeWidgetModifier(makeTransformer(0.0f)),
            makeWidgetModifier(makeTransformer(-1.0f))
            );
}

template <typename T>
auto transition(Transition<T> transition)
{
    return makeWidgetModifier([](auto widget, BuildParams const& params,
                Transition<T> transition)
    {
        auto transitionedBuilder = std::move(transition.transitioned)(
                btl::clone(widget)
                )(params);

        return std::move(widget)
            | std::move(transition.active)
            | makeSharedInstanceSignalModifier(
                [](auto instance, auto transitionedBuilder)
                {
                    auto size = share(map(&Instance::getSize, instance));
                    auto transitionedInstance = std::move(transitionedBuilder)
                        (size)
                        .getInstance()
                        ;

                    auto newRenderTree = group(instance, std::move(transitionedInstance))
                            .map([=](auto const& activeInstance,
                                        auto const& transitionedInstance)
                            {
                                auto transition = std::make_shared<avg::TransitionNode>(
                                        avg::Obb(),
                                        true,
                                        activeInstance.getRenderTree().getRoot(),
                                        transitionedInstance.getRenderTree().getRoot()
                                        );

                                return avg::RenderTree(std::move(transition));
                            });

                    return std::move(instance)
                        | setRenderTree(std::move(newRenderTree))
                        ;
                },
                std::move(transitionedBuilder)
                )
            ;
    },
    provideBuildParams(),
    std::move(transition)
    );
}

} // namespace reactive::widget

