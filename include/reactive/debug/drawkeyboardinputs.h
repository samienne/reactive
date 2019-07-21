#pragma once

#include "reactive/widgetfactory.h"

#include "reactive/signal/map.h"

#include "reactive/shapes.h"

#include <avg/pathbuilder.h>

#include <pmr/new_delete_resource.h>

namespace reactive::debug
{
    namespace detail
    {
        inline avg::Drawing makeRect(DrawContext const& drawContext, avg::Obb obb)
        {
            float w = obb.getSize().x();
            float h = obb.getSize().y();

            return obb.getTransform() * avg::Drawing(
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
        return widget::onDraw<DrawContextTag, KeyboardInputTag>([]
            (DrawContext const& drawContext, auto const& inputs)
        {
            avg::Drawing result;

            for (auto&& input : inputs)
            {
                result += detail::makeRect(drawContext, input.getObb());
            }

            return result;
        });
    }
} // reactive::debug

