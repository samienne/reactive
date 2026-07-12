#include <bqui/shape/shape.h>
#include <bqui/shape/rectangle.h>

#include <bqui/widget/widget.h>

#include <bq/signal/signal.h>

#include <avg/brush.h>
#include <avg/pen.h>
#include <avg/color.h>
#include <avg/transform.h>
#include <avg/animated.h>
#include <avg/curve/curves.h>
#include <avg/vector.h>

#include <gtest/gtest.h>

#include <optional>

using namespace bqui;

namespace
{
    avg::Brush testBrush()
    {
        return avg::Brush(avg::Color());
    }

    avg::Pen testPen()
    {
        return avg::Pen(avg::Brush(avg::Color()), 1.0f);
    }

    auto angleSignal()
    {
        return bq::signal::constant(avg::infiniteAnimation(
                -0.1f, 0.1f, avg::curve::easeInOutCubic, 2.0f,
                avg::RepeatMode::reverse));
    }

    auto offsetSignal()
    {
        return bq::signal::constant(avg::infiniteAnimation(
                avg::Vector2f(-20, 0), avg::Vector2f(20, 0),
                avg::curve::easeInOutCubic, 2.0f, avg::RepeatMode::reverse));
    }
} // anonymous namespace

// These tests exist to force-instantiate every Shape builder method. The
// builder methods are templates that are never compiled unless called, so a
// broken method (e.g. referencing a non-existent avg::Shape operator, or a
// missing ref-qualifier) compiles fine on its own and only fails when used.
// Exercising them here turns that into a compile-time failure.

TEST(shape, geometryTransformsInstantiate)
{
    using bq::signal::constant;
    using shape::rectangle;

    // Value convenience overloads.
    (void)(rectangle().translate(avg::Vector2f(10, 20)));
    (void)(rectangle().rotate(0.5f));
    (void)(rectangle().rotate(0.5f, avg::Vector2f(0.0f, 0.0f)));
    (void)(rectangle().scale(2.0f));
    (void)(rectangle().scale(2.0f, avg::Vector2f(0.0f, 0.0f)));
    (void)(rectangle().transform(avg::translate(1.0f, 2.0f)));

    // Signal overloads (explicitly animated values).
    (void)(rectangle().translate(offsetSignal()));
    (void)(rectangle().rotate(angleSignal()));
    (void)(rectangle().scale(constant(avg::Animated<float>(2.0f))));
    (void)(rectangle().transform(
            constant(avg::Animated<avg::Transform>(avg::Transform()))));

    // Sizing (draw-time).
    (void)(rectangle().size(constant(avg::Vector2f(100, 100))));
    (void)(rectangle().size(constant(avg::Vector2f(100, 100)),
            constant(avg::Vector2f(0.5f, 0.5f))));

    // Boolean op and stroke-widening (both -> Shape).
    (void)(rectangle().clip(rectangle()));
    (void)(rectangle().strokeToShape(constant(testPen())));

    // Conversion to AnyShape.
    shape::AnyShape anyShape = rectangle().rotate(0.5f);
    (void)anyShape;

    SUCCEED();
}

TEST(shape, terminalStylingInstantiate)
{
    using bq::signal::constant;
    using shape::rectangle;

    widget::AnyWidget w1 = rectangle().fill(testBrush());
    widget::AnyWidget w2 = rectangle().fill(avg::Color());
    widget::AnyWidget w3 = rectangle().fill(constant(testBrush()));
    widget::AnyWidget w4 = rectangle().stroke(testPen());
    widget::AnyWidget w5 = rectangle().stroke(constant(testPen()));
    widget::AnyWidget w6 = rectangle().fillAndStroke(
            constant(testBrush()), constant(testPen()));
    widget::AnyWidget w7 = rectangle().fillAndStroke(
            std::nullopt, constant(testPen()));
    widget::AnyWidget w8 = rectangle().fillAndStroke(
            constant(testBrush()), std::nullopt);

    (void)w1;
    (void)w2;
    (void)w3;
    (void)w4;
    (void)w5;
    (void)w6;
    (void)w7;
    (void)w8;

    SUCCEED();
}
