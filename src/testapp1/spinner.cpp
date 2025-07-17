#include "spinner.h"

#include <bqui/modifier/setsize.h>
#include <bqui/modifier/ondraw.h>
#include <bqui/modifier/elementmodifier.h>

#include <bqui/provider/providetheme.h>

#include <bqui/shapes.h>

#include <avg/animated.h>
#include <avg/curve/curves.h>

using namespace bqui;

namespace {
    auto drawSpinner(avg::DrawContext const& context, avg::Vector2f size,
            Theme const& theme, float state)
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

            auto shape = avg::Shape(makeCircle(context.getResource(),
                        ase::Vector2f(0.0f, 0.0f), s/2.0f))
                .fill(brush);

            float a = 6.28f / 10.0f * (float) i;
            drawing += std::move(shape)
                .transform(avg::translate(std::cos(a) * w, std::sin(a) * w));
        }

        return std::move(drawing)
            .transform(avg::translate(0.5f*size[0], 0.5f*size[1]))
            ;
    }
} // anonymous namespace

widget::AnyWidget spinner()
{
    auto state = bq::signal::constant(avg::infiniteAnimation(
                0.0f, 1.0f, avg::curve::linear, 2.0f
                ));

    return widget::makeWidget()
        | modifier::onDraw(drawSpinner, provider::provideTheme(), std::move(state))
        | modifier::setSize({ 100.0f, 100.0f })
        ;
}

