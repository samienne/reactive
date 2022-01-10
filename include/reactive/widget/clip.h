#pragma once

#include "instancemodifier.h"
#include "builder.h"

namespace reactive::widget
{
    inline auto clip()
    {
        return makeInstanceModifier([](Instance instance)
            {
                auto clip = std::make_shared<avg::ClipNode>(
                        instance.getObb(),
                        instance.getRenderTree().getRoot()
                        );

                auto areas = instance.getInputAreas();
                for (auto&& area : areas)
                    area = std::move(area).clip(instance.getObb());

                auto obbTInverse = instance.getObb().getTransform().inverse();
                avg::Rect obbRect(avg::Vector2f(0.0f, 0.0f), instance.getSize());

                auto inputs = instance.getKeyboardInputs();

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

                return std::move(instance)
                    .setKeyboardInputs(std::move(inputs))
                    .setInputAreas(std::move(areas))
                    .setRenderTree(avg::RenderTree(std::move(clip)))
                    ;
            });
    }
} // reactive::widget

