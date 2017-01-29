#pragma once

#include "margin.h"
#include "theme.h"

#include "reactive/widget.h"
#include "reactive/widgetfactory.h"

#include "reactive/rendering.h"

#include "reactive/signal/constant.h"
#include "reactive/signal.h"

#include <avg/transform.h>

#include <iostream>

namespace reactive
{
    namespace widget
    {
        inline auto frame()
        {
            auto f = [](avg::Vector2f size, widget::Theme const& theme)
                -> avg::Drawing
            {
                auto pen = avg::Pen(avg::Brush(theme.getBackgroundHighlight()),
                        1.0f);

                auto brush = avg::Brush(theme.getBackground());

                auto shape =  makeShape(
                        makeRect(size[0] - 5.0f, size[1] - 5.0f),
                        btl::none,
                        btl::just(pen));

                return avg::Transform()
                    .translate(0.5f * size[0],
                            0.5f * size[1])
                    * avg::Drawing(shape);
            };

            return
                margin(signal::constant(5.0f))
                >> mapFactoryWidget(onDrawBehind<avg::Vector2f, widget::Theme>(
                            std::move(f)))
                ;
        }
    }
}
