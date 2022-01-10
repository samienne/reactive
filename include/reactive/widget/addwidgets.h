#pragma once

#include "reduce.h"

#include "widgetmodifier.h"
#include "instance.h"

#include "reactive/inputarea.h"

#include "reactive/signal/combine.h"
#include "reactive/signal/mbind.h"

#include <btl/all.h>
#include <btl/typetraits.h>
#include <btl/sequence.h>
#include <btl/cloneoncopy.h>
#include <btl/bundle.h>

#include <type_traits>

namespace reactive::widget
{
    inline auto addWidgets(Instance i, std::vector<Instance> const& instances)
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

    inline auto addWidgets(std::vector<AnySignal<Instance>> instances)
    {
        return makeWidgetModifier([](Instance instance, auto instances)
                {
                    return addWidgets(std::move(instance), std::move(instances));
                },
                combine(std::move(instances))
                );

    }


    template <typename T>
    auto addWidgets(Signal<T, std::vector<Instance>> instances)
    {
        return makeWidgetModifier([](Instance instance, auto instances)
                {
                    return addWidgets(std::move(instance), std::move(instances));
                },
                std::move(instances)
                );
    }

    inline auto addWidget(AnySignal<Instance> instance)
    {
        std::vector<AnySignal<Instance>> instances;
        instances.push_back(std::move(instance));

        return addWidgets(std::move(instances));
    }
} // namespace reactive::widget

