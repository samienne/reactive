#include <avg/animationoptions.h>
#include <avg/curve/curves.h>
#include <avg/drawcontext.h>
#include <avg/pathbuilder.h>
#include <avg/rect.h>
#include <avg/shape.h>
#include <avg/shapefunction.h>
#include <avg/vector.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <chrono>

namespace
{

avg::Shape box(avg::DrawContext const& context, float width, float height)
{
    return avg::Shape(context.pathBuilder()
            .start(0.0f, 0.0f)
            .lineTo(width, 0.0f)
            .lineTo(width, height)
            .lineTo(0.0f, height)
            .close()
            .build());
}

} // anonymous namespace

TEST(ShapeFunction, aChangeOfShapeKindTakesTheNewShape)
{
    avg::DrawContext context(pmr::new_delete_resource());

    auto square = avg::makeShapeFunction(
            [](avg::DrawContext const& context, avg::Vector2f, float side)
            {
                return box(context, side, side);
            },
            10.0f);

    auto wide = avg::makeShapeFunction(
            [](avg::DrawContext const& context, avg::Vector2f, float width,
                float height)
            {
                return box(context, width, height);
            },
            20.0f, 5.0f);

    auto updated = square.updated(wide,
            avg::AnimationOptions{ std::chrono::milliseconds(100),
                avg::curve::linear },
            std::chrono::milliseconds(0));

    auto bounds = updated(std::chrono::milliseconds(50), context,
            avg::Vector2f(0.0f, 0.0f)).getControlBb();

    EXPECT_FLOAT_EQ(20.0f, bounds.getWidth());
    EXPECT_FLOAT_EQ(5.0f, bounds.getHeight());
}
