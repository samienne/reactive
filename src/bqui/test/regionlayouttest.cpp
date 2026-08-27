#include "widget/constraintlayout.h"

#include <bqui/modifier/instancemodifier.h>
#include <bqui/modifier/setsizehint.h>
#include <bqui/modifier/widgetmodifier.h>

#include <bqui/widget/vbox.h>
#include <bqui/widget/widget.h>

#include <bqui/buildparams.h>
#include <bqui/inputarea.h>
#include <bqui/provider/provideparam.h>
#include <bqui/simplesizehint.h>
#include <bqui/sizehint.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/constant.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <arrange/expression.h>
#include <arrange/strength.h>
#include <arrange/variable.h>

#include <avg/obb.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <btl/uniqueid.h>

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

using namespace bqui;
using namespace bqui::widget;
using namespace bq::signal;

namespace
{

struct Geometry
{
    avg::Vector2f position;
    avg::Vector2f size;
};

// A leaf that fixes its size hint and tags its realised instance with an
// InputArea, so its window-space geometry can be read back through the
// accumulated transform, exactly as the layout tests probe geometry.
AnyWidget probe(btl::UniqueId id, Band width, Band height)
{
    return makeWidget()
        | modifier::makeWidgetModifier(modifier::makeInstanceModifier(
                    [](Instance instance, btl::UniqueId id)
                    {
                        auto areas = instance.getInputAreas();
                        areas.push_back(makeInputArea(id, instance.getObb()));

                        return std::move(instance)
                            .setInputAreas(std::move(areas));
                    }, constant(id)))
        | modifier::setSizeHint(constant(SizeHint(simpleSizeHint(width, height))))
        ;
}

Geometry readProbe(Instance const& instance, btl::UniqueId id)
{
    for (auto const& area : instance.getInputAreas())
        if (area.getId() == id)
            return Geometry{
                area.getTransform().getTranslation(),
                area.getObbs().front().getSize()
            };

    ADD_FAILURE() << "probe was not realised";
    return {};
}

Instance realise(AnyWidget widget, avg::Vector2f size)
{
    auto instanceSignal = std::move(widget)(BuildParams())(constant(size))
        .getInstance();

    auto context = makeSignalContext(std::move(instanceSignal));
    return context.evaluate<0>().get<0>();
}

// The Axis::y, cross-fill fragment a plain column contributes, replicating the
// container's own constraints (constraintbox.cpp's makeBoxSpec) so both the
// per-container oracle and the region solve stack the identical relations. When
// @p anchor is set the container is pinned to a fixed rectangle (a region root);
// otherwise its box is left for an enclosing container to place, and only its
// children are tied to it, which is what lets a nested column join one region.
LayoutSpec columnFragment(BoxVariables const& container,
        std::vector<BoxVariables> const& boxes,
        std::vector<SizeHint> const& hints, avg::Vector2f size, bool anchor)
{
    LayoutSpec spec;

    auto push = [&spec](std::vector<arrange::Constraint> constraints)
    {
        for (auto& constraint : constraints)
            spec.constraints.push_back(std::move(constraint));
    };

    if (anchor)
        push(anchorConstraints(container, 0.0f, 0.0f, size[0], size[1]));

    push(boxConstraints(container, boxes, Axis::y));

    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        AxisHint band = hints[i].getHeightForWidth(size[0]);
        arrange::Expression childExtent = boxes[i].height();

        spec.constraints.push_back(
                (childExtent >= arrange::Expression(band.extent.min))
                | arrange::Strength::strong());
        spec.constraints.push_back(
                (childExtent <= arrange::Expression(band.extent.max))
                | arrange::Strength::strong());
        spec.constraints.push_back(
                (childExtent == arrange::Expression(band.extent.natural))
                | arrange::Strength::medium());

        AxisHint crossBand = hints[i].getWidthForHeight(size[1]);
        placeInSlot(spec.constraints, boxes[i].left, boxes[i].right,
                container.left, container.right, 0.5f, crossBand.extent.max);
    }

    for (BoxVariables const& box : boxes)
    {
        spec.variables.push_back(box.left);
        spec.variables.push_back(box.top);
        spec.variables.push_back(box.right);
        spec.variables.push_back(box.bottom);
    }

    return spec;
}

// The whole region solves in one absolute top-down space aligned with the
// window, so every box, at any nesting depth, flips into window-space y-up the
// same way — no per-container composition. This is the region solve's payoff and
// the equality it must reproduce against the per-container oracle.
Geometry windowGeometry(LayoutSolution const& solution, BoxVariables const& box,
        float windowHeight)
{
    avg::Obb solved = readObb(solution, box);
    avg::Vector2f size = solved.getSize();
    avg::Vector2f topLeft = solved.getTransform().getTranslation();

    return Geometry{
        avg::Vector2f(topLeft[0], windowHeight - (topLeft[1] + size[1])),
        size
    };
}

void expectSame(std::string const& label, Geometry const& region,
        Geometry const& oracle)
{
    SCOPED_TRACE(label);

    EXPECT_FLOAT_EQ(oracle.position[0], region.position[0]);
    EXPECT_FLOAT_EQ(oracle.position[1], region.position[1]);
    EXPECT_FLOAT_EQ(oracle.size[0], region.size[0]);
    EXPECT_FLOAT_EQ(oracle.size[1], region.size[1]);

    // The mechanism's headline claim is byte-for-byte equality; report where the
    // merged solve diverges from the per-container one if it ever does.
    EXPECT_EQ(0, std::memcmp(&region, &oracle, sizeof(Geometry)))
        << "region solve diverged from the per-container oracle at the bit level";
}

Band const fixed100 = { 100.0f, 100.0f, 100.0f };
Band const fixed60 = { 60.0f, 60.0f, 60.0f };
Band const fixed40 = { 40.0f, 40.0f, 40.0f };

} // namespace

// One solve spanning an outer column and a column nested inside it reproduces
// the geometry the shipped per-container path produces, where each container
// runs its own solve. The outer column holds a leaf and the inner column; the
// inner column holds two leaves. The region owner (layoutRegion) merges the two
// columns' fragments into a single tableau, and every leaf lands exactly where
// the real, decoupled vbox nesting places it.
TEST(RegionLayout, oneSolveSpansNestedColumnsAndMatchesPerContainer)
{
    avg::Vector2f const window(100.0f, 200.0f);

    // The per-container oracle: the real vbox nesting, each container its own
    // firewall and solve, read back through the accumulated instance transforms.
    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idC = btl::makeUniqueId();
    btl::UniqueId const idD = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> inner;
    inner.push_back(probe(idC, fixed100, fixed60));
    inner.push_back(probe(idD, fixed100, fixed40));

    std::vector<ArraySignal<AnyWidget>> outer;
    outer.push_back(probe(idA, fixed100, fixed100));
    outer.push_back(vbox(ArraySignal<AnyWidget>(std::move(inner))));

    Instance instance = realise(
            vbox(ArraySignal<AnyWidget>(std::move(outer))), window);
    Geometry oracleA = readProbe(instance, idA);
    Geometry oracleC = readProbe(instance, idC);
    Geometry oracleD = readProbe(instance, idD);

    // The region: the same containers' constraints, but merged into one solve.
    // O is the outer column (a region root, anchored to the window); B is both
    // the outer's second child and the inner column's own container, so the two
    // fragments couple on the one box.
    BoxVariables O;
    BoxVariables A;
    BoxVariables B;
    BoxVariables C;
    BoxVariables D;

    SizeHint const hintA = simpleSizeHint(fixed100, fixed100);
    SizeHint const hintB = simpleSizeHint(fixed100, fixed100);
    SizeHint const hintC = simpleSizeHint(fixed100, fixed60);
    SizeHint const hintD = simpleSizeHint(fixed100, fixed40);

    std::vector<LayoutSpec> fragments;
    fragments.push_back(columnFragment(O, { A, B }, { hintA, hintB },
                window, true));
    fragments.push_back(columnFragment(B, { C, D }, { hintC, hintD },
                avg::Vector2f(100.0f, 100.0f), false));

    auto solutionSignal = layoutRegion(
            AnySignal<std::vector<LayoutSpec>>(constant(std::move(fragments))));
    auto context = makeSignalContext(std::move(solutionSignal));
    LayoutSolution solution = context.evaluate<0>().get<0>();

    Geometry regionA = windowGeometry(solution, A, window[1]);
    Geometry regionC = windowGeometry(solution, C, window[1]);
    Geometry regionD = windowGeometry(solution, D, window[1]);

    // Expected absolute geometry, independent of either path: A fills the top
    // 100, the inner column fills the bottom 100 with C above D.
    EXPECT_FLOAT_EQ(0.0f, regionA.position[0]);
    EXPECT_FLOAT_EQ(100.0f, regionA.position[1]);
    EXPECT_FLOAT_EQ(40.0f, regionC.position[1]);
    EXPECT_FLOAT_EQ(60.0f, regionC.size[1]);
    EXPECT_FLOAT_EQ(0.0f, regionD.position[1]);
    EXPECT_FLOAT_EQ(40.0f, regionD.size[1]);

    expectSame("leaf A", regionA, oracleA);
    expectSame("leaf C (nested)", regionC, oracleC);
    expectSame("leaf D (nested)", regionD, oracleD);
}

// The down-channel a region owner provides: a LayoutSolution set on the build
// params is read back intact through provideParam<LayoutSolutionTag>, so a
// participating box can read its own obb out of the region's one solution.
TEST(RegionLayout, layoutSolutionTagCarriesSolutionDown)
{
    BoxVariables box;
    LayoutSolution solution;
    solution.emplace(box.left.id(), 7.0);
    solution.emplace(box.right.id(), 42.0);

    BuildParams params;
    params.set<LayoutSolutionTag>(constant(solution));

    auto signal = provider::provideParam<LayoutSolutionTag>()(params);
    auto context = makeSignalContext(std::move(signal));
    LayoutSolution received = context.evaluate<0>().get<0>();

    avg::Obb obb = readObb(received, box);
    EXPECT_FLOAT_EQ(35.0f, obb.getSize()[0]);
}
