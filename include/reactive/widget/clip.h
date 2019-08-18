#pragma once

#include "reactive/widgetmap.h"
#include "reactive/widgetfactory.h"

namespace reactive::widget
{
    inline auto clipDrawing()
    {
        return makeWidgetMap<ObbTag, DrawingTag>(
                [](avg::Obb obb, avg::Drawing d)
                {
                    return std::move(d).clip(obb);
                });
    }

    inline auto clipInputAreas()
    {
        return makeWidgetMap<ObbTag, InputAreaTag>(
                [](avg::Obb obb, std::vector<InputArea> areas)
                {
                    for (auto&& area : areas)
                        area = std::move(area).clip(obb);

                    return areas;
                });
    }

    inline auto clipKeyboardInputs()
    {
        return makeWidgetMap<ObbTag, KeyboardInputTag>(
                [](avg::Obb obb, std::vector<KeyboardInput> inputs)
                {
                    auto obbTInverse = obb.getTransform().inverse();
                    avg::Rect obbRect(avg::Vector2f(0.0f, 0.0f), obb.getSize());

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

                    return inputs;
                });
    }

    inline auto clip()
    {
        return makeWidgetMap()
            .map(clipDrawing())
            .map(clipInputAreas())
            .map(clipKeyboardInputs())
            ;
    }
} // reactive::widget

