#include "curvevisualizer.h"

#include <bqui/modifier/buildermodifier.h>
#include <bqui/modifier/margin.h>
#include <bqui/modifier/setanimation.h>
#include <bqui/modifier/ondraw.h>
#include <bqui/modifier/setsizehint.h>
#include <bqui/modifier/frame.h>

#include <bqui/shapes.h>

#include <avg/curve/curves.h>
#include <avg/rendertree.h>
#include <avg/drawcontext.h>

using namespace bqui;

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

        return std::move(path).stroke(avg::Pen(avg::Brush(curveColor)))
            + context.pathBuilder()
                .start(0.0f, margin)
                .lineTo(size[0], margin)
                .stroke(avg::Pen(avg::Brush(axisColor)))
            + context.pathBuilder()
                .start(0.0f, margin + graphHeight)
                .lineTo(size[0], margin + graphHeight)
                .stroke(avg::Pen(avg::Brush(axisColor)))
            ;
    }
} // anonymous namespace

widget::AnyWidget curveVisualizer(
        bq::signal::AnySignal<avg::Curve> curve
        )
{
    auto c = curve.clone().map([](auto curve)
            {
                return avg::Animated<avg::Curve>(std::move(curve));
            });

    return widget::makeWidget()
        | modifier::onDraw(drawGraph, std::move(c))
        | modifier::setAnimation(0.9f, avg::curve::easeInOutCubic, std::move(curve))
        | modifier::margin(bq::signal::constant(7.0f))
        | modifier::frame()
        | modifier::setSizeHint(bq::signal::constant(simpleSizeHint(300.0f, 300.0f)))
        ;
}

