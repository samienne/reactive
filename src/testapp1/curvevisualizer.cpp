#include "curvevisualizer.h"
#include "avg/curve/curves.h"
#include "reactive/widget/buildermodifier.h"
#include "reactive/widget/margin.h"

#include <reactive/widget/setanimation.h>
#include <reactive/widget/ondraw.h>
#include <reactive/widget/setsizehint.h>
#include <reactive/widget/frame.h>

#include <reactive/shapes.h>

#include <avg/rendertree.h>
#include <avg/drawcontext.h>

using namespace reactive;

namespace {
    auto drawGraph(avg::DrawContext const& context, avg::Vector2f const& size,
            std::function<float(float)> curve)
    {
        avg::Color curveColor = avg::Color(0.1f, 0.5f, 0.1f);
        avg::Color axisColor = avg::Color(1.0f, 1.0f, 1.0f, 0.1f);

        float marginPercentage = 0.2f;
        float graphHeightPercentage = 1.0f - 2.0f * marginPercentage;

        float graphHeight = graphHeightPercentage * size[1];
        float margin = marginPercentage * size[1];

        auto path = context.pathBuilder()
            .start(0.0f, curve(0.0f) * graphHeight + margin)
            ;

        float step = 1.0f;

        for (float i = 1; i < std::ceil(size[0]); i += 1.0f)
        {
            path = std::move(path)
                .lineTo(i * step, curve(i / size[0]) * graphHeight + margin)
                ;
        }

        return context.drawing(
                makeShape(
                    std::move(path).build(),
                    std::nullopt,
                    std::make_optional(avg::Pen(avg::Brush(curveColor)))
                    )
                )
            + makeShape(
                    context.pathBuilder()
                    .start(0.0f, margin)
                    .lineTo(size[0], margin)
                    .build()
                    ,
                    std::nullopt,
                    std::make_optional(avg::Pen(avg::Brush(axisColor)))
                    )
            + makeShape(
                    context.pathBuilder()
                    .start(0.0f, margin + graphHeight)
                    .lineTo(size[0], margin + graphHeight)
                    .build()
                    ,
                    std::nullopt,
                    std::make_optional(avg::Pen(avg::Brush(axisColor)))
                    )
            ;
    }
} // anonymous namespace

reactive::widget::AnyWidget curveVisualizer(
        reactive::AnySignal<avg::Curve> curve
        )
{
    auto c = std::move(curve).map([](auto curve)
            {
                return avg::Animated<avg::Curve>(std::move(curve));
            });

    return widget::makeWidget()
        | widget::onDraw(drawGraph, std::move(c))
        | widget::setAnimation(0.9f, avg::curve::easeInOutCubic)
        | widget::margin(signal::constant(7.0f))
        | widget::frame()
        | widget::setSizeHint(signal::constant(simpleSizeHint(300.0f, 300.0f)))
        ;
}

