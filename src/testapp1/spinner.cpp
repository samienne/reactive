#include "spinner.h"

#include <reactive/widgetmaps.h>

#include <reactive/widget/theme.h>

#include <reactive/widget/frame.h>

#include <reactive/signal/loop.h>
#include <reactive/signal/time.h>
#include <reactive/rendering.h>

#include <avg/drawing.h>

#include <iostream>

using namespace reactive;

WidgetFactory makeSpinner()
{
    auto draw = [](ase::Vector2f size, widget::Theme const& theme,
            std::chrono::duration<float> t)
        -> avg::Drawing
        {
            avg::Brush brush(theme.getGreen());

            float step = 2.0f / 10.0f;
            float w = std::min(size[0], size[1]) * 0.5f - 15.0f;

            avg::Drawing drawing;
            for (int i = 0; i < 10; ++i)
            {
                float shift = step * (float)i;
                float tt = (t.count() + shift) / 2.0f;
                if (tt > 1.0f)
                    tt -= 1.0f;

                // (x - 0.33)(x-0.66) = x^2 -0.66x - 0.33x + 2/9
                float s = std::max(0.0f,
                        (-tt * tt + tt - 2.0f/9.0f)) * 200.0f + 10.0f;
                auto shape = makeShape(
                        makeRect(s, s),
                        btl::just(brush),
                        btl::none);

                float a = 6.28f / 10.0f * (float) i;
                drawing += avg::Drawing(std::move(shape))
                    .transform(avg::Transform()
                            .translate(cos(a) * w, sin(a) * w));
            }

            /*drawing += makeShape(makeRect(200.0f, 20.0f),
                    btl::just(avg::Brush(avg::Color(0.15, 0.06, 0.24, 0.3))),
                    btl::none);*/

            return std::move(drawing)
                .transform(avg::Transform()
                        .translate(0.5f*size[0], 0.5f*size[1]));
        };

    auto t = signal::loop(signal::time(), std::chrono::microseconds(2000000));

    return makeWidgetFactory()
        | onDraw<ase::Vector2f, widget::Theme>(draw, std::move(t))
        | setSizeHint(signal::constant(simpleSizeHint(150.0f, 150.0f)));
}

