#include "bqui/modifier/drawkeyboardinputs.h"

#include "bqui/modifier/ondraw.h"
#include "bqui/modifier/elementmodifier.h"

#include "bqui/provider/providebuildparams.h"

#include <avg/pathbuilder.h>

namespace bqui::modifier
{

namespace
{
    avg::Drawing makeRect(avg::DrawContext const& drawContext, avg::Obb obb)
    {
        float w = obb.getSize().x();
        float h = obb.getSize().y();

        return obb.getTransform() *
            drawContext.pathBuilder()
                .start(ase::Vector2f(.0f, .0f))
                .lineTo(ase::Vector2f(w, .0f))
                .lineTo(ase::Vector2f(w, h))
                .lineTo(ase::Vector2f(.0f, h))
                .close()
                .stroke(avg::Pen())
                ;
    }
} // anonymous namespace

AnyWidgetModifier drawKeyboardInputs()
{
    return makeWidgetModifier([](auto widget, auto const& params)
        {
            return std::move(widget)
                | modifier::makeWidgetModifier(makeSharedElementModifier(
                    [](auto element, auto const& params)
                    {
                        auto inputs = element.getKeyboardInputs();
                        auto size = element.getSize();

                        auto drawnWidget = makeWidgetFromElement(std::move(element))
                            | modifier::onDraw([](avg::DrawContext const& context,
                                        avg::Vector2f const&,
                                        auto const& inputs
                                        )
                                {
                                    auto result = context.drawing();

                                    for (auto&& input : inputs)
                                    {
                                        result += makeRect(context, input.getObb());
                                    }

                                    return result;
                                },
                                std::move(inputs)
                                );

                        return std::move(drawnWidget)(params)(std::move(size));
                    },
                    params
                ));
        },
        provider::provideBuildParams()
        );
}

}

