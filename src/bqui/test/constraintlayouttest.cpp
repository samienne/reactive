#include "widget/constraintlayout.h"

#include <bq/signal/constant.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/input.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <arrange/expression.h>
#include <arrange/strength.h>

#include <gtest/gtest.h>

#include <vector>

using namespace bqui::widget;
using namespace bq::signal;

namespace
{
    void append(std::vector<arrange::Constraint>& into,
            std::vector<arrange::Constraint> const& from)
    {
        into.insert(into.end(), from.begin(), from.end());
    }

    LayoutSolution solveOnce(LayoutSpec spec)
    {
        auto context = makeSignalContext(
                solveLayout(AnySignal<LayoutSpec>(constant(std::move(spec)))));

        return context.evaluate<0>().get<0>();
    }
} // namespace

// A vertical box lays its children top to bottom: the first child sits at the
// container top, each following child begins where the previous ended, and the
// last reaches the container bottom. Sizes come from the children's own
// contributions, so a strong preferred height on the first child leaves the
// rest for the second.
TEST(constraintLayout, verticalBoxStacksChildren)
{
    BoxVariables container;
    BoxVariables a;
    BoxVariables b;

    LayoutSpec spec;
    append(spec.constraints, anchorConstraints(container, 0.f, 0.f, 100.f, 60.f));
    append(spec.constraints, boxConstraints(container, { a, b }, Axis::vertical));
    spec.constraints.push_back(
            (a.height() == arrange::Expression(20.0)) | arrange::Strength::strong());

    spec.variables = { a.left, a.top, a.right, a.bottom,
        b.left, b.top, b.right, b.bottom };

    auto solution = solveOnce(std::move(spec));

    EXPECT_DOUBLE_EQ(0.0, solution.at(a.top.id()));
    EXPECT_DOUBLE_EQ(20.0, solution.at(a.bottom.id()));
    EXPECT_DOUBLE_EQ(20.0, solution.at(b.top.id()));
    EXPECT_DOUBLE_EQ(60.0, solution.at(b.bottom.id()));
    EXPECT_DOUBLE_EQ(0.0, solution.at(a.left.id()));
    EXPECT_DOUBLE_EQ(100.0, solution.at(a.right.id()));

    avg::Obb obbA = readObb(solution, a);
    EXPECT_FLOAT_EQ(100.0f, obbA.getSize()[0]);
    EXPECT_FLOAT_EQ(20.0f, obbA.getSize()[1]);
}

// A horizontal box is the same story rotated: children run left to right and a
// strong preferred width on the first leaves the rest for the second.
TEST(constraintLayout, horizontalBoxStacksChildren)
{
    BoxVariables container;
    BoxVariables a;
    BoxVariables b;

    LayoutSpec spec;
    append(spec.constraints, anchorConstraints(container, 0.f, 0.f, 90.f, 30.f));
    append(spec.constraints, boxConstraints(container, { a, b }, Axis::horizontal));
    spec.constraints.push_back(
            (a.width() == arrange::Expression(30.0)) | arrange::Strength::strong());

    spec.variables = { a.left, a.right, b.left, b.right, a.top, a.bottom };

    auto solution = solveOnce(std::move(spec));

    EXPECT_DOUBLE_EQ(0.0, solution.at(a.left.id()));
    EXPECT_DOUBLE_EQ(30.0, solution.at(a.right.id()));
    EXPECT_DOUBLE_EQ(30.0, solution.at(b.left.id()));
    EXPECT_DOUBLE_EQ(90.0, solution.at(b.right.id()));
    EXPECT_DOUBLE_EQ(0.0, solution.at(a.top.id()));
    EXPECT_DOUBLE_EQ(30.0, solution.at(a.bottom.id()));
}

// A stack overlays its children: each child gets the container's whole box.
TEST(constraintLayout, stackOverlaysChildren)
{
    BoxVariables container;
    BoxVariables a;
    BoxVariables b;

    LayoutSpec spec;
    append(spec.constraints, anchorConstraints(container, 10.f, 20.f, 110.f, 220.f));
    append(spec.constraints, stackConstraints(container, { a, b }));

    spec.variables = { a.left, a.top, a.right, a.bottom,
        b.left, b.top, b.right, b.bottom };

    auto solution = solveOnce(std::move(spec));

    for (BoxVariables const& box : { a, b })
    {
        EXPECT_DOUBLE_EQ(10.0, solution.at(box.left.id()));
        EXPECT_DOUBLE_EQ(20.0, solution.at(box.top.id()));
        EXPECT_DOUBLE_EQ(110.0, solution.at(box.right.id()));
        EXPECT_DOUBLE_EQ(220.0, solution.at(box.bottom.id()));
    }
}

// A required minimum outranks a strong preferred size: the first child prefers
// 20 but is required to be at least 30, so it takes 30 and the second fills the
// remaining 30.
TEST(constraintLayout, requiredMinimumBeatsStrongPreferred)
{
    BoxVariables container;
    BoxVariables a;
    BoxVariables b;

    LayoutSpec spec;
    append(spec.constraints, anchorConstraints(container, 0.f, 0.f, 100.f, 60.f));
    append(spec.constraints, boxConstraints(container, { a, b }, Axis::vertical));
    spec.constraints.push_back(
            (a.height() == arrange::Expression(20.0)) | arrange::Strength::strong());
    spec.constraints.push_back(a.height() >= arrange::Expression(30.0));

    spec.variables = { a.bottom, b.top, b.bottom };

    auto solution = solveOnce(std::move(spec));

    EXPECT_DOUBLE_EQ(30.0, solution.at(a.bottom.id()));
    EXPECT_DOUBLE_EQ(30.0, solution.at(b.top.id()));
    EXPECT_DOUBLE_EQ(60.0, solution.at(b.bottom.id()));
}

// The solver is fold state: when the constraints change the same solver
// re-solves and every reader sees the new geometry. Growing the container
// widens both children and hands the freed vertical space to the filler child.
TEST(constraintLayout, resolvesWhenConstraintsChange)
{
    BoxVariables container;
    BoxVariables a;
    BoxVariables b;

    auto describe = [&](float width, float height)
    {
        LayoutSpec spec;
        append(spec.constraints,
                anchorConstraints(container, 0.f, 0.f, width, height));
        append(spec.constraints,
                boxConstraints(container, { a, b }, Axis::vertical));
        spec.constraints.push_back(
                (a.height() == arrange::Expression(20.0)) | arrange::Strength::strong());
        spec.variables = { a.right, a.bottom, b.top, b.bottom };
        return spec;
    };

    auto input = makeInput(describe(100.f, 60.f));
    auto context = makeSignalContext(solveLayout(AnySignal<LayoutSpec>(input.signal)));

    auto before = context.evaluate<0>().get<0>();
    EXPECT_DOUBLE_EQ(100.0, before.at(a.right.id()));
    EXPECT_DOUBLE_EQ(60.0, before.at(b.bottom.id()));
    EXPECT_DOUBLE_EQ(40.0, before.at(b.bottom.id()) - before.at(b.top.id()));

    input.handle.set(describe(200.f, 100.f));
    context.update(FrameInfo(1, {}));

    auto after = context.evaluate<0>().get<0>();
    EXPECT_DOUBLE_EQ(200.0, after.at(a.right.id()));
    EXPECT_DOUBLE_EQ(20.0, after.at(a.bottom.id()));
    EXPECT_DOUBLE_EQ(100.0, after.at(b.bottom.id()));
    EXPECT_DOUBLE_EQ(80.0, after.at(b.bottom.id()) - after.at(b.top.id()));
}
