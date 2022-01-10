#pragma once

#include "reactive/widget/ondraw.h"
#include "reactive/widget/instancemodifier.h"
#include "reactive/widget/builder.h"

#include "reactive/shapes.h"

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
        return widget::makeSharedInstanceSignalModifier([](auto widget)
            {
                auto inputs = signal::map(&widget::Instance::getKeyboardInputs, widget);

                return std::move(widget)
                    | widget::onDraw([](avg::DrawContext const& context,
                                avg::Vector2f const&, auto const& inputs)
                        {
                            auto result = context.drawing();

                            for (auto&& input : inputs)
                            {
                                result += detail::makeRect(context, input.getObb());
                            }

                            return result;
                        },
                        std::move(inputs)
                        )
                    ;
            });
    }
} // reactive::debug

