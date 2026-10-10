#include "widget/constraintlayout.h"

#include <bqui/widget/builder.h>
#include <bqui/widget/widget.h>

#include <bqui/simplesizehint.h>
#include <bqui/sizehint.h>

#include <bq/signal/constant.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/input.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <arrange/expression.h>
#include <arrange/strength.h>

#include <gtest/gtest.h>

#include <vector>

using namespace bqui;
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

    // boxConstraints() now tiles only the main axis and leaves the cross axis
    // for placeInSlot(); a test focused on the main-axis stack pins the cross
    // edges to the container itself so its boxes read back as full rectangles.
    void spanCross(std::vector<arrange::Constraint>& into,
            BoxVariables const& box, BoxVariables const& container, Axis axis)
    {
        if (axis == Axis::y)
        {
            into.push_back(
                    arrange::Expression(box.left)
                    == arrange::Expression(container.left));
            into.push_back(
                    arrange::Expression(box.right)
                    == arrange::Expression(container.right));
        }
        else
        {
            into.push_back(
                    arrange::Expression(box.top)
                    == arrange::Expression(container.top));
            into.push_back(
                    arrange::Expression(box.bottom)
                    == arrange::Expression(container.bottom));
        }
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
    append(spec.constraints, boxConstraints(container, { a, b }, Axis::y));
    spanCross(spec.constraints, a, container, Axis::y);
    spanCross(spec.constraints, b, container, Axis::y);
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
    append(spec.constraints, boxConstraints(container, { a, b }, Axis::x));
    spanCross(spec.constraints, a, container, Axis::x);
    spanCross(spec.constraints, b, container, Axis::x);
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

// The solver matches a widget's constraints from pass to pass by the arrange
// ids of its box variables alone, so those ids have to survive the transforms a
// builder goes through on its way into a container: a size-hint change and the
// erasure to AnyBuilder. Were either to mint fresh ids, the diff would treat the
// same widget as a departure and an arrival every frame.
TEST(constraintLayout, boxVariableIdsSurviveHintChangeAndErasure)
{
    auto builder = makeBuilder();
    BoxVariables const original = builder.getBoxVariables();

    auto rehinted = std::move(builder).setSizeHint(
            bq::signal::constant(simpleSizeHint(10.0f, 10.0f)));
    BoxVariables const afterHint = rehinted.getBoxVariables();

    AnyBuilder erased = std::move(rehinted);
    BoxVariables const afterErasure = erased.getBoxVariables();

    EXPECT_EQ(original.left.id(), afterHint.left.id());
    EXPECT_EQ(original.top.id(), afterHint.top.id());
    EXPECT_EQ(original.right.id(), afterHint.right.id());
    EXPECT_EQ(original.bottom.id(), afterHint.bottom.id());

    EXPECT_EQ(original.left.id(), afterErasure.left.id());
    EXPECT_EQ(original.top.id(), afterErasure.top.id());
    EXPECT_EQ(original.right.id(), afterErasure.right.id());
    EXPECT_EQ(original.bottom.id(), afterErasure.bottom.id());
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
    append(spec.constraints, boxConstraints(container, { a, b }, Axis::y));
    spec.constraints.push_back(
            (a.height() == arrange::Expression(20.0)) | arrange::Strength::strong());
    spec.constraints.push_back(a.height() >= arrange::Expression(30.0));

    spec.variables = { a.bottom, b.top, b.bottom };

    auto solution = solveOnce(std::move(spec));

    EXPECT_DOUBLE_EQ(30.0, solution.at(a.bottom.id()));
    EXPECT_DOUBLE_EQ(30.0, solution.at(b.top.id()));
    EXPECT_DOUBLE_EQ(60.0, solution.at(b.bottom.id()));
}

// A box that cannot hold its content overflows rather than squeezing: the last
// child keeps its own size and extends past the container's end. Both children
// are held firmly at 30, so their 60 of content does not fit the 40-tall
// container; the trailing weak fill pull loses to the second child's size and
// its bottom lands at 60, twenty past the container.
TEST(constraintLayout, overFullBoxOverflowsInsteadOfSqueezing)
{
    BoxVariables container;
    BoxVariables a;
    BoxVariables b;

    LayoutSpec spec;
    append(spec.constraints, anchorConstraints(container, 0.f, 0.f, 100.f, 40.f));
    append(spec.constraints, boxConstraints(container, { a, b }, Axis::y));
    spec.constraints.push_back(
            (a.height() == arrange::Expression(30.0)) | arrange::Strength::strong());
    spec.constraints.push_back(
            (b.height() == arrange::Expression(30.0)) | arrange::Strength::strong());

    spec.variables = { a.top, a.bottom, b.top, b.bottom };

    auto solution = solveOnce(std::move(spec));

    EXPECT_DOUBLE_EQ(0.0, solution.at(a.top.id()));
    EXPECT_DOUBLE_EQ(30.0, solution.at(a.bottom.id()));
    EXPECT_DOUBLE_EQ(30.0, solution.at(b.top.id()));
    EXPECT_DOUBLE_EQ(60.0, solution.at(b.bottom.id()));
}

// The weak fill pull is kept, so a child free to grow still fills the surplus:
// the first child is held at 20 and the second, unconstrained, stretches to the
// container's bottom.
TEST(constraintLayout, fillerFillsSurplus)
{
    BoxVariables container;
    BoxVariables a;
    BoxVariables b;

    LayoutSpec spec;
    append(spec.constraints, anchorConstraints(container, 0.f, 0.f, 100.f, 80.f));
    append(spec.constraints, boxConstraints(container, { a, b }, Axis::y));
    spec.constraints.push_back(
            (a.height() == arrange::Expression(20.0)) | arrange::Strength::strong());

    spec.variables = { a.bottom, b.top, b.bottom };

    auto solution = solveOnce(std::move(spec));

    EXPECT_DOUBLE_EQ(20.0, solution.at(a.bottom.id()));
    EXPECT_DOUBLE_EQ(20.0, solution.at(b.top.id()));
    EXPECT_DOUBLE_EQ(80.0, solution.at(b.bottom.id()));
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
                boxConstraints(container, { a, b }, Axis::y));
        spanCross(spec.constraints, a, container, Axis::y);
        spanCross(spec.constraints, b, container, Axis::y);
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
