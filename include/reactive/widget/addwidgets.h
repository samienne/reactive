#pragma once

#include "reduce.h"

#include "widgetmodifier.h"

#include "reactive/inputarea.h"
#include "reactive/widget.h"

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
    inline auto addWidgets(Widget w, std::vector<Widget> const& widgets)
    {
        auto container = std::make_shared<avg::ContainerNode>(
                w.getObb()
                //avg::Obb(w.getObb().getSize())
                );

        container->addChild(w.getRenderTree().getRoot());

        for (auto const& widget : widgets)
        {
            container->addChild(widget.getRenderTree().getRoot());
        }

        std::vector<InputArea> areas = w.getInputAreas();
        for (auto const& widget : widgets)
            for (auto const& area : widget.getInputAreas())
                areas.push_back(area);

        std::vector<KeyboardInput> inputs = w.getKeyboardInputs();
        for (auto const& widget : widgets)
            for (auto const& input : widget.getKeyboardInputs())
                if (input.isFocusable())
                    inputs.push_back(input);

        return std::move(w)
            .setRenderTree(avg::RenderTree(std::move(container)))
            .setInputAreas(std::move(areas))
            .setKeyboardInputs(std::move(inputs))
            ;
    }

    inline auto addWidgets(std::vector<AnySignal<Widget>> widgets)
    {
        return makeWidgetModifier([](Widget widget, auto widgets)
                {
                    return addWidgets(std::move(widget), std::move(widgets));
                },
                combine(std::move(widgets))
                );

    }


    template <typename T>
    auto addWidgets(Signal<T, std::vector<Widget>> widgets)
    {
        return makeWidgetModifier([](Widget widget, auto widgets)
                {
                    return addWidgets(std::move(widget), std::move(widgets));
                },
                std::move(widgets)
                );
    }

    inline auto addWidget(AnySignal<Widget> widget)
    {
        std::vector<AnySignal<Widget>> widgets;
        widgets.push_back(std::move(widget));

        return addWidgets(std::move(widgets));
    }
} // namespace reactive::widget

