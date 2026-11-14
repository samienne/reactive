#pragma once

#include "instancemodifier.h"

#include "bqui/widget/instance.h"
#include "bqui/widget/reduce.h"

#include "bqui/inputarea.h"

#include <bq/signal/signal.h>
#include <bq/signal/combine.h>

#include <avg/rendertree/containernode.h>

#include <btl/all.h>
#include <btl/typetraits.h>
#include <btl/sequence.h>
#include <btl/cloneoncopy.h>

#include <type_traits>

namespace bqui::modifier
{
    /**
     * @brief Adds the instances' render trees, input areas, focusable
     * inputs and introspection to the instance, after its own.
     *
     * A flat container is for children laid out side by side; a stacked one
     * paints each child over the ones before it, which is what a modifier
     * that draws behind or over its widget is.
     */
    inline auto addWidgets(widget::Instance i,
            std::vector<widget::Instance> const& instances,
            avg::ContainerNode::Stacking stacking =
                avg::ContainerNode::Stacking::flat)
    {
        auto container = std::make_shared<avg::ContainerNode>(
                i.getObb(),
                stacking
                );

        container->addChild(i.getRenderTree().getRoot());

        for (auto const& instance : instances)
        {
            container->addChild(instance.getRenderTree().getRoot());
        }

        std::vector<InputArea> areas = i.getInputAreas();
        for (auto const& instance : instances)
            for (auto const& area : instance.getInputAreas())
                areas.push_back(area);

        std::vector<KeyboardInput> inputs = i.getKeyboardInputs();
        for (auto const& instance : instances)
            for (auto const& input : instance.getKeyboardInputs())
                if (input.isFocusable())
                    inputs.push_back(input);

        widget::Introspection introspection = i.getIntrospection();
        introspection.children.reserve(instances.size());
        for (auto const& instance : instances)
            introspection.children.push_back(
                    widget::makeIntrospectionChild(instance.getIntrospection()));

        return std::move(i)
            .setRenderTree(avg::RenderTree(std::move(container)))
            .setInputAreas(std::move(areas))
            .setKeyboardInputs(std::move(inputs))
            .setIntrospection(std::move(introspection))
            ;
    }

    inline auto addWidgets(
            std::vector<bq::signal::AnySignal<widget::Instance>> instances,
            avg::ContainerNode::Stacking stacking =
                avg::ContainerNode::Stacking::flat)
    {
        return makeInstanceModifier([stacking](widget::Instance instance,
                    auto instances)
                {
                    return addWidgets(std::move(instance),
                            std::move(instances), stacking);
                },
                bq::signal::combine(std::move(instances))
                );

    }


    template <typename T>
    auto addWidgets(
            bq::signal::Signal<T, std::vector<widget::Instance>> instances,
            avg::ContainerNode::Stacking stacking =
                avg::ContainerNode::Stacking::flat)
    {
        return makeInstanceModifier([stacking](widget::Instance instance,
                    auto instances)
                {
                    return addWidgets(std::move(instance),
                            std::move(instances), stacking);
                },
                std::move(instances)
                );
    }

    inline auto addWidget(bq::signal::AnySignal<widget::Instance> instance,
            avg::ContainerNode::Stacking stacking =
                avg::ContainerNode::Stacking::flat)
    {
        std::vector<bq::signal::AnySignal<widget::Instance>> instances;
        instances.push_back(std::move(instance));

        return addWidgets(std::move(instances), stacking);
    }
}
