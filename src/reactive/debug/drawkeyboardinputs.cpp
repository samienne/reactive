#include "debug/drawkeyboardinputs.h"

#include "widget/ondraw.h"
#include "widget/providebuildparams.h"
#include "widget/elementmodifier.h"
#include "widget/builder.h"

#include "shapes.h"

#include "signal/map.h"

#include <avg/pathbuilder.h>

namespace reactive::debug
{

namespace
{
    avg::Drawing makeRect(avg::DrawContext const& drawContext, avg::Obb obb)
    {
        float w = obb.getSize().x();
        float h = obb.getSize().y();

        return obb.getTransform() * drawContext.drawing(
                drawContext.pathBuilder()
                    .start(ase::Vector2f(.0f, .0f))
                    .lineTo(ase::Vector2f(w, .0f))
                    .lineTo(ase::Vector2f(w, h))
                    .lineTo(ase::Vector2f(.0f, h))
                    .close()
                    .buildShape(avg::Pen())
                    );
    }
} // anonymous namespace

widget::AnyWidgetModifier drawKeyboardInputs()
{
    return widget::makeWidgetModifier([](auto widget, auto const& params)
        {
            return std::move(widget)
                | makeWidgetModifier(widget::makeSharedElementModifier(
                    [](auto element, auto const& params)
                    {
                        auto inputs = element.getKeyboardInputs();
                        auto size = element.getSize();

                        auto drawnWidget = makeWidgetFromElement(std::move(element))
                            | widget::onDraw([](avg::DrawContext const& context,
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
        widget::provideBuildParams()
        );
}

} // namespace reactive::debug

