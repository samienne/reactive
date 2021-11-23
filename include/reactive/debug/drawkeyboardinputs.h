#pragma once

#include "reactive/widget/ondrawcustom.h"
#include "reactive/widget/bindkeyboardinputs.h"
#include "reactive/widget/widgettransformer.h"

#include "reactive/shapes.h"
#include "reactive/widgetfactory.h"

#include "reactive/signal/map.h"

#include <avg/pathbuilder.h>

namespace reactive::debug
{
    namespace detail
    {
        inline avg::Drawing makeRect(avg::DrawContext const& drawContext, avg::Obb obb)
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
    } // detail

    inline auto drawKeyboardInputs()
    {
        return widget::makeWidgetTransformer()
            /*
            .compose(widget::bindKeyboardInputs())
            .bind(widget::onDrawCustom(
            [](avg::DrawContext const& drawContext, avg::Obb const&,
                auto const& inputs)
            {
                {
                    auto result = drawContext.drawing();

                    for (auto&& input : inputs)
                    {
                        result += detail::makeRect(drawContext, input.getObb());
                    }

                    return result;
                }
            }))
            */
            ;
    }
} // reactive::debug

