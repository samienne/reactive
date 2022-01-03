#pragma once

#include "widgetmodifier.h"

#include "reactive/widgetfactory.h"

namespace reactive::widget
{
    inline auto clipRenderTree()
    {
        return makeWidgetModifier([](Widget widget)
            {
                auto clip = std::make_shared<avg::ClipNode>(
                        widget.getObb(),
                        widget.getRenderTree().getRoot()
                        );

                return std::move(widget)
                    .setRenderTree(avg::RenderTree(std::move(clip)))
                    ;
            });
    }

    inline auto clipInputAreas()
    {
        return makeWidgetModifier([](auto widget)
            {
                auto areas = widget.getInputAreas();
                for (auto&& area : areas)
                    area = std::move(area).clip(widget.getObb());

                return std::move(widget)
                    .setInputAreas(std::move(areas))
                    ;
            });
    }

    inline auto clipKeyboardInputs()
    {
        return makeWidgetModifier([](auto widget)
            {
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
                    ;
            });
    }

    inline auto clip()
    {
        return makeWidgetTransformer()
            .compose(
                    clipRenderTree(),
                    clipInputAreas(),
                    clipKeyboardInputs()
                    )
            ;
    }
} // reactive::widget

