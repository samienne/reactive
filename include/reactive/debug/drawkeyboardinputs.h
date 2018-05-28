#pragma once

#include "reactive/widgetfactory.h"

#include "reactive/signal/map.h"

namespace reactive
{
    namespace debug
    {
        namespace detail
        {
            inline avg::Drawing makeRect(avg::Obb obb)
            {
                float w = obb.getSize().x();
                float h = obb.getSize().y();

                return obb.getTransform() * avg::Drawing(makeShape(
                            avg::Path(avg::PathSpec()
                                .start(ase::Vector2f(.0f, .0f))
                                .lineTo(ase::Vector2f(w, .0f))
                                .lineTo(ase::Vector2f(w, h))
                                .lineTo(ase::Vector2f(.0f, h))
                                .close()
                                ),
                            btl::none,
                            btl::just(avg::Pen())));
            }
        } // detail

        inline auto drawKeyboardInputs()
        {
            return onDraw<KeyboardInputTag>([]
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
    } // debug
} // reactive

