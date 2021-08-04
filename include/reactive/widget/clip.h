#pragma once

#include "bindobb.h"
#include "bindinputareas.h"
#include "bindkeyboardinputs.h"
#include "setinputareas.h"
#include "setkeyboardinputs.h"
#include "widgettransformer.h"

#include "reactive/widgetfactory.h"

namespace reactive::widget
{
    /*
    inline auto clipDrawing()
    {
        return makeWidgetTransformer()
            .compose(bindObb(), grabDrawing())
            .bind([](auto obb, auto drawing)
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
    */

    inline auto clipInputAreas()
    {
        return makeWidgetTransformer()
            .compose(bindObb(), grabInputAreas())
            .bind([](auto obb, auto inputAreas)
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
        return makeWidgetTransformer()
            .compose(bindObb(), grabKeyboardInputs())
            .bind([](auto obb, auto keyboardInputs)
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
        return makeWidgetTransformer()
            .compose(
                    //clipDrawing(),
                    clipInputAreas(),
                    clipKeyboardInputs()
                    )
            ;
    }
} // reactive::widget

