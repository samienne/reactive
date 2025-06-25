#pragma once

#include "instancemodifier.h"

#include "bqui/widget/instance.h"
#include "bqui/widget/reduce.h"

#include "bqui/inputarea.h"

#include <bq/signal/signal.h>
#include <bq/signal/combine.h>

#include <btl/all.h>
#include <btl/typetraits.h>
#include <btl/sequence.h>
#include <btl/cloneoncopy.h>

#include <type_traits>

namespace bqui::modifier
{
    inline auto addWidgets(widget::Instance i,
            std::vector<widget::Instance> const& instances)
    {
        auto container = std::make_shared<avg::ContainerNode>(
                i.getObb()
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

        return std::move(i)
            .setRenderTree(avg::RenderTree(std::move(container)))
            .setInputAreas(std::move(areas))
            .setKeyboardInputs(std::move(inputs))
            ;
    }

    inline auto addWidgets(std::vector<bq::signal::AnySignal<widget::Instance>> instances)
    {
        return makeInstanceModifier([](widget::Instance instance, auto instances)
                {
                    return addWidgets(std::move(instance), std::move(instances));
                },
                bq::signal::combine(std::move(instances))
                );

    }


    template <typename T>
    auto addWidgets(bq::signal::Signal<T, std::vector<widget::Instance>> instances)
    {
        return makeInstanceModifier([](widget::Instance instance, auto instances)
                {
                    return addWidgets(std::move(instance), std::move(instances));
                },
                std::move(instances)
                );
    }

    inline auto addWidget(bq::signal::AnySignal<widget::Instance> instance)
    {
        std::vector<bq::signal::AnySignal<widget::Instance>> instances;
        instances.push_back(std::move(instance));

        return addWidgets(std::move(instances));
    }
}

