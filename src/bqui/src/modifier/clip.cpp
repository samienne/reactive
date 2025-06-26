#include "bqui/modifier/clip.h"

#include "bqui/modifier/instancemodifier.h"
#include "bqui/modifier/widgetmodifier.h"

#include "bqui/widget/builder.h"

namespace bqui::modifier
{

AnyWidgetModifier clip()
{
    return makeWidgetModifier(makeInstanceModifier([](widget::Instance instance)
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
        }));
}

}
