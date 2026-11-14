#include <avg/pathbuilder.h>
#include <avg/rect.h>
#include <avg/shape.h>
#include <avg/vector.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

namespace
{

avg::Shape unitSquare()
{
    return avg::Shape(avg::PathBuilder(pmr::new_delete_resource())
            .start(0.0f, 0.0f)
            .lineTo(1.0f, 0.0f)
            .lineTo(1.0f, 1.0f)
            .lineTo(0.0f, 1.0f)
            .close()
            .build());
}

void expectSpan(avg::Rect const& r, float left, float right)
{
    EXPECT_FLOAT_EQ(left, r.getLeft());
    EXPECT_FLOAT_EQ(right, r.getRight());
}

} // anonymous namespace

TEST(Shape, controlBbOfAnAddAppliesEachPlacementOnce)
{
    auto shape = unitSquare().add(
            unitSquare().translate(avg::Vector2f(10.0f, 0.0f)));

    expectSpan(shape.getControlBb(), 0.0f, 11.0f);
}

TEST(Shape, controlBbOfAnIntersectAppliesEachPlacementOnce)
{
    auto shape = unitSquare().translate(avg::Vector2f(0.5f, 0.0f))
        .intersect(unitSquare());

    expectSpan(shape.getControlBb(), 0.5f, 1.0f);
}

TEST(Shape, controlBbOfASubtractIsTheMinuend)
{
    auto shape = unitSquare().translate(avg::Vector2f(3.0f, 0.0f))
        .subtract(unitSquare());

    expectSpan(shape.getControlBb(), 3.0f, 4.0f);
}

TEST(Shape, controlBbComposesAnOuterPlacementOverTheInnerOnes)
{
    auto shape = unitSquare()
        .add(unitSquare().translate(avg::Vector2f(10.0f, 0.0f)))
        .translate(avg::Vector2f(5.0f, 0.0f));

    expectSpan(shape.getControlBb(), 5.0f, 16.0f);
}

TEST(Shape, addingToAPlacedAddKeepsItsPlacement)
{
    auto shape = unitSquare()
        .add(unitSquare().translate(avg::Vector2f(10.0f, 0.0f)))
        .translate(avg::Vector2f(5.0f, 0.0f))
        .add(unitSquare());

    expectSpan(shape.getControlBb(), 0.0f, 16.0f);
}

TEST(Shape, intersectingAPlacedIntersectKeepsItsPlacement)
{
    auto shape = unitSquare()
        .intersect(unitSquare())
        .translate(avg::Vector2f(5.0f, 0.0f))
        .intersect(unitSquare().translate(avg::Vector2f(5.5f, 0.0f)));

    expectSpan(shape.getControlBb(), 5.5f, 6.0f);
}
