#include "spinner.h"

#include <reactive/widget/clip.h>
#include <reactive/widget/theme.h>
#include <reactive/widget/frame.h>
#include <reactive/widget/widgettransformer.h>

#include <reactive/simplesizehint.h>

#include <reactive/signal/loop.h>
#include <reactive/signal/time.h>

#include <avg/drawing.h>

#include <pmr/new_delete_resource.h>

#include <iostream>

using namespace reactive;

namespace
{
    auto draw(DrawContext const& drawContext, ase::Vector2f size,
            widget::Theme const& theme, std::chrono::duration<float> t)
        -> avg::Drawing
        {
            avg::Brush brush(theme.getGreen());

            float step = 2.0f / 10.0f;
            float w = std::min(size[0], size[1]) * 0.5f - 15.0f;

            auto drawing = drawContext.drawing();

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
                        makeCircle(drawContext.getResource(),
                            ase::Vector2f(0.0f, 0.0f), s/2.0f),
                        btl::just(brush),
                        btl::none);

                float a = 6.28f / 10.0f * (float) i;
                drawing += drawContext.drawing(std::move(shape))
                    .transform(avg::translate(std::cos(a) * w, std::sin(a) * w));
            }

            return std::move(drawing)
                .transform(avg::translate(0.5f*size[0], 0.5f*size[1]))
                ;
        }
} // anonymous namespace

WidgetFactory makeSpinner()
{
    using namespace reactive::widget;

    auto t = signal::loop(signal::time(), std::chrono::microseconds(2000000));

    return makeWidgetFactory()
        | makeWidgetTransformer()
        .compose(bindDrawContext(), bindSize(), bindTheme())
        .values(std::move(t))
        .bind(onDraw(draw))
        | widget::clip()
        | setSizeHint(signal::constant(simpleSizeHint(150.0f, 150.0f)))
        ;
}

