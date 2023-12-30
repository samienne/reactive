#include "spinner.h"

#include <reactive/widget/ondraw.h>
#include <reactive/widget/providetheme.h>
#include <reactive/widget/elementmodifier.h>
#include <reactive/shapes.h>

#include <avg/animated.h>
#include <avg/curve/curves.h>

using namespace reactive;

namespace {
    auto drawSpinner(avg::DrawContext const& context, avg::Vector2f size,
            widget::Theme const& theme, float state)
    {
        avg::Brush brush(theme.getGreen());

        float step = 1.0f / 10.0f;
        float w = std::min(size[0], size[1]) * 0.5f - 15.0f;

        auto drawing = context.drawing();

        for (int i = 0; i < 10; ++i)
        {
            float shift = step * (float)i;
            float tt = (state + shift);
            if (tt > 1.0f)
                tt -= 1.0f;

            // (x - 0.33)(x-0.66) = x^2 -0.66x - 0.33x + 2/9
            float s = std::max(0.0f,
                    (-tt * tt + tt - 2.0f/9.0f)) * 200.0f + 10.0f;

            auto shape = makeShape(
                    makeCircle(context.getResource(),
                        ase::Vector2f(0.0f, 0.0f), s/2.0f),
                    std::make_optional(brush),
                    std::nullopt);

            float a = 6.28f / 10.0f * (float) i;
            drawing += context.drawing(std::move(shape))
                .transform(avg::translate(std::cos(a) * w, std::sin(a) * w));
        }

        return std::move(drawing)
            .transform(avg::translate(0.5f*size[0], 0.5f*size[1]))
            ;
    }
} // anonymous namespace

reactive::widget::AnyWidget spinner()
{
    auto state = signal::constant(avg::infiniteAnimation(
                0.0f, 1.0f, avg::curve::linear, 2.0f
                ));

    return widget::makeWidget()
        | widget::onDraw(drawSpinner, widget::provideTheme(), std::move(state))
        ;
}

