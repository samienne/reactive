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
        inline avg::Drawing makeRect(avg::Obb obb)
        {
            float w = obb.getSize().x();
            float h = obb.getSize().y();

            return obb.getTransform() * avg::Drawing(makeShape(
                        avg::PathBuilder(pmr::new_delete_resource())
                            .start(ase::Vector2f(.0f, .0f))
                            .lineTo(ase::Vector2f(w, .0f))
                            .lineTo(ase::Vector2f(w, h))
                            .lineTo(ase::Vector2f(.0f, h))
                            .close()
                            .build(),
                        btl::none,
                        btl::just(avg::Pen())));
        }
    } // detail

    inline auto drawKeyboardInputs()
    {
        return widget::onDraw<KeyboardInputTag>([]
            (auto const& inputs)
        {
            avg::Drawing result;

            for (auto&& input : inputs)
            {
                result += detail::makeRect(input.getObb());
            }

            return result;
        });
    }
} // reactive::debug

