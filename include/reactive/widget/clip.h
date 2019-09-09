#pragma once

#include "bindkeyboardinputs.h"
#include "setkeyboardinputs.h"
#include "binddrawing.h"
#include "setdrawing.h"

#include "reactive/widgetmap.h"
#include "reactive/widgetfactory.h"

namespace reactive::widget
{
    inline auto clipDrawing()
    {
        return makeWidgetMap()
            .provide(bindObb(), grabDrawing())
            .bindWidgetMap([](auto obb, auto drawing)
            {
                auto newDrawing = signal::map(
                    [](avg::Obb obb, avg::Drawing d)
                    {
                        return std::move(d).clip(obb);
                    },
                    std::move(obb),
                    std::move(drawing)
                    );

                return setDrawing(std::move(newDrawing));
            });
    }

    inline auto clipInputAreas()
    {
        return makeWidgetMap()
            .provide(bindObb(), grabInputAreas())
            .bindWidgetMap([](auto obb, auto inputAreas)
            {
                auto newAreas = signal::map(
                    [](avg::Obb obb, std::vector<InputArea> areas)
                    {
                        for (auto&& area : areas)
                            area = std::move(area).clip(obb);

                        return areas;
                    },
                    std::move(obb),
                    std::move(inputAreas)
                    );

                return setInputAreas(std::move(newAreas));
            });
    }

    inline auto clipKeyboardInputs()
    {
        return makeWidgetMap()
            .provide(bindObb(), grabKeyboardInputs())
            .bindWidgetMap([](auto obb, auto keyboardInputs)
            {
                auto newKeyboardInputs = signal::map(
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
                    },
                    std::move(obb),
                    std::move(keyboardInputs)
                    );

                return setKeyboardInputs(std::move(newKeyboardInputs));
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

