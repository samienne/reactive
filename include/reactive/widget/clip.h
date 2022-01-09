#pragma once

#include "widgetmodifier.h"

#include "reactive/widgetfactory.h"

namespace reactive::widget
{
    inline auto clip()
    {
        return makeWidgetModifier([](auto widget)
            {
                auto clip = std::make_shared<avg::ClipNode>(
                        widget.getObb(),
                        widget.getRenderTree().getRoot()
                        );

                auto areas = widget.getInputAreas();
                for (auto&& area : areas)
                    area = std::move(area).clip(widget.getObb());

                auto obbTInverse = widget.getObb().getTransform().inverse();
                avg::Rect obbRect(avg::Vector2f(0.0f, 0.0f), widget.getSize());

                auto inputs = widget.getKeyboardInputs();

                inputs.erase(
                        std::remove_if(
                            inputs.begin(),
                            inputs.end(),
                            [&](KeyboardInput const& input)
                            {
                                return !(obbTInverse * input.getObb())
                                    .getBoundingRect()
                                    .overlaps(obbRect);
                            }),
                        inputs.end()
                        );

                return std::move(widget)
                    .setKeyboardInputs(std::move(inputs))
                    .setInputAreas(std::move(areas))
                    .setRenderTree(avg::RenderTree(std::move(clip)))
                    ;
            });
    }
} // reactive::widget

