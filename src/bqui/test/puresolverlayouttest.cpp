#include "widget/constraintbox.h"
#include "widget/constraintlayout.h"

#include <bqui/modifier/instancemodifier.h>
#include <bqui/modifier/setsizehint.h>
#include <bqui/modifier/widgetmodifier.h>

#include <bqui/widget/vbox.h>
#include <bqui/widget/widget.h>

#include <bqui/buildparams.h>
#include <bqui/inputarea.h>
#include <bqui/simplesizehint.h>
#include <bqui/sizehint.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/constant.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <arrange/expression.h>
#include <arrange/variable.h>

#include <avg/obb.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <btl/uniqueid.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
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
// accumulated transform. The pure-solver path ignores the hint entirely, so a
// probe's band is set deliberately away from the weak 100 default to prove the
// band is never read.
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

// A region owner's solve settles a pass behind the build, so a few update passes
// are driven before the geometry is read.
Instance realiseConverged(AnyWidget widget, avg::Vector2f size)
{
    auto instanceSignal = std::move(widget)(BuildParams())(constant(size))
        .getInstance();

    auto context = makeSignalContext(std::move(instanceSignal));
    Instance instance = context.evaluate<0>().get<0>();

    for (uint64_t frame = 1; frame <= 5; ++frame)
    {
        context.update(bq::signal::FrameInfo(frame,
                    std::chrono::microseconds(0)));
        instance = context.evaluate<0>().get<0>();
    }

    return instance;
}

Band const fixed100 = { 100.0f, 100.0f, 100.0f };
Band const fixed40 = { 40.0f, 40.0f, 40.0f };

// A synthetic pure-solver leaf whose height is a step function of its resolved
// width -- 30px for every 50px width band -- emitted as a required
// height == f(width) constraint. The step is genuinely non-linear: only the
// staged pipeline, where pass 2 sees pass 1's resolved width as a concrete
// value, can express it; a single linear x+y solve cannot compute ceil(). The
// horizontal accessor pins the leaf's width, so the value driving f in pass 2 is
// known, but f reads it back off the pass-1 solution rather than the literal.
BoxDescriptor stepHeightLeaf(BoxVariables box, float width)
{
    auto horizontal = constant(std::vector<arrange::Constraint>{
            arrange::Expression(box.left) == arrange::Expression(0.0),
            arrange::Expression(box.right)
                == arrange::Expression(static_cast<double>(width))
            });

    return BoxDescriptor(box, BoxDescriptor::Constraints(std::move(horizontal)),
            [box](AnySignal<float> width)
            {
                return BoxDescriptor::Constraints(std::move(width).map(
                        [box](float w) -> std::vector<arrange::Constraint>
                        {
                            float height = 30.0f * std::ceil(w / 50.0f);
                            return {
                                arrange::Expression(box.top)
                                    == arrange::Expression(0.0),
                                arrange::Expression(box.bottom)
                                    == arrange::Expression(box.top)
                                        + arrange::Expression(
                                                static_cast<double>(height))
                            };
                        }));
            });
}

// Concatenates a fixed set of per-box constraint vectors into one fragment
// solved on its own axis, reading the named variables back.
LayoutSpec makeFragment(std::vector<arrange::Constraint> const& a,
        std::vector<arrange::Constraint> const& b,
        std::vector<arrange::Variable> variables)
{
    LayoutSpec spec;
    spec.constraints = a;
    spec.constraints.insert(spec.constraints.end(), b.begin(), b.end());
    spec.variables = std::move(variables);
    return spec;
}

AnySignal<std::vector<LayoutSpec>> oneFragment(AnySignal<LayoutSpec> spec)
{
    return AnySignal<std::vector<LayoutSpec>>(std::move(spec).map(
                [](LayoutSpec const& spec)
                {
                    return std::vector<LayoutSpec>{ spec };
                }));
}

} // namespace

// A box with no constraint but the universal weak defaults resolves to 100x100.
// The defaults sit at the strictly-weakest tier, so on an axis nothing else
// pins they are the only pull and settle the extent at 100 exactly, keeping the
// solve well-posed rather than leaving a free degree of freedom.
TEST(PureSolverLayout, unconstrainedBoxIsHundredSquare)
{
    BoxVariables box;

    LayoutSpec spec;
    // Pin the origin so the position is definite; the size is left entirely to
    // the weak defaults.
    spec.constraints.push_back(
            arrange::Expression(box.left) == arrange::Expression(0.0));
    spec.constraints.push_back(
            arrange::Expression(box.top) == arrange::Expression(0.0));
    spec.constraints.push_back(weakWidthDefault(box));
    spec.constraints.push_back(weakHeightDefault(box));
    spec.variables = { box.left, box.top, box.right, box.bottom };

    std::vector<LayoutSpec> fragments{ std::move(spec) };
    auto solutionSignal = layoutRegion(
            AnySignal<std::vector<LayoutSpec>>(constant(std::move(fragments))));
    auto context = makeSignalContext(std::move(solutionSignal));
    LayoutSolution solution = context.evaluate<0>().get<0>();

    avg::Obb obb = readObb(solution, box);
    EXPECT_FLOAT_EQ(100.0f, obb.getSize()[0]);
    EXPECT_FLOAT_EQ(100.0f, obb.getSize()[1]);
    EXPECT_FLOAT_EQ(0.0f, obb.getTransform().getTranslation()[0]);
    EXPECT_FLOAT_EQ(0.0f, obb.getTransform().getTranslation()[1]);
}

// A real nested vbox behind pureSolverRoot lays out with pure constraints plus
// the weak defaults and no band read: every leaf, whatever its SizeHint band,
// comes out 100x100, stacked edge to edge. The outer column holds a leaf and the
// inner column; the inner holds two leaves. The window is sized so the three
// 100-tall leaves stack exactly, top to bottom: A, then C, then D.
TEST(PureSolverLayout, nestedColumnsLayOutByDefaultsIgnoringBands)
{
    avg::Vector2f const window(100.0f, 300.0f);

    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idC = btl::makeUniqueId();
    btl::UniqueId const idD = btl::makeUniqueId();

    // The probes carry a 40x40 band the pure path must ignore.
    std::vector<ArraySignal<AnyWidget>> inner;
    inner.push_back(probe(idC, fixed40, fixed40));
    inner.push_back(probe(idD, fixed40, fixed40));

    std::vector<ArraySignal<AnyWidget>> outer;
    outer.push_back(probe(idA, fixed40, fixed40));
    outer.push_back(vbox(ArraySignal<AnyWidget>(std::move(inner))));

    Instance instance = realiseConverged(
            pureSolverRoot(vbox(ArraySignal<AnyWidget>(std::move(outer)))),
            window);

    Geometry a = readProbe(instance, idA);
    Geometry c = readProbe(instance, idC);
    Geometry d = readProbe(instance, idD);

    // Every leaf defaults to 100x100 despite its 40x40 band.
    for (Geometry const& g : { a, c, d })
    {
        EXPECT_FLOAT_EQ(100.0f, g.size[0]);
        EXPECT_FLOAT_EQ(100.0f, g.size[1]);
        EXPECT_FLOAT_EQ(0.0f, g.position[0]);
    }

    // Stacked top to bottom in window y-up space: A on top, then C, then D.
    EXPECT_FLOAT_EQ(200.0f, a.position[1]);
    EXPECT_FLOAT_EQ(100.0f, c.position[1]);
    EXPECT_FLOAT_EQ(0.0f, d.position[1]);
}

// Where a band-free column should match the banded one, it does. Two leaves
// whose band is exactly the weak default (100x100) in a window that fits them
// both: the pure path (ON, pureSolverRoot) places them identically to the
// shipped per-container banded path (OFF, a plain vbox).
TEST(PureSolverLayout, matchesBandedWhereLayoutIsBandFree)
{
    avg::Vector2f const window(100.0f, 200.0f);

    btl::UniqueId const idTop = btl::makeUniqueId();
    btl::UniqueId const idBottom = btl::makeUniqueId();

    auto makeTree = [&]() -> AnyWidget
    {
        std::vector<ArraySignal<AnyWidget>> children;
        children.push_back(probe(idTop, fixed100, fixed100));
        children.push_back(probe(idBottom, fixed100, fixed100));
        return vbox(ArraySignal<AnyWidget>(std::move(children)));
    };

    Instance off = realiseConverged(makeTree(), window);
    Geometry offTop = readProbe(off, idTop);
    Geometry offBottom = readProbe(off, idBottom);

    Instance on = realiseConverged(pureSolverRoot(makeTree()), window);
    Geometry onTop = readProbe(on, idTop);
    Geometry onBottom = readProbe(on, idBottom);

    EXPECT_FLOAT_EQ(offTop.position[0], onTop.position[0]);
    EXPECT_FLOAT_EQ(offTop.position[1], onTop.position[1]);
    EXPECT_FLOAT_EQ(offTop.size[0], onTop.size[0]);
    EXPECT_FLOAT_EQ(offTop.size[1], onTop.size[1]);

    EXPECT_FLOAT_EQ(offBottom.position[0], onBottom.position[0]);
    EXPECT_FLOAT_EQ(offBottom.position[1], onBottom.position[1]);
    EXPECT_FLOAT_EQ(offBottom.size[0], onBottom.size[0]);
    EXPECT_FLOAT_EQ(offBottom.size[1], onBottom.size[1]);

    // The absolute geometry both paths agree on: top leaf fills the upper half,
    // bottom leaf the lower half.
    EXPECT_FLOAT_EQ(100.0f, onTop.position[1]);
    EXPECT_FLOAT_EQ(0.0f, onBottom.position[1]);
}

// The two-phase solve stages pass 2 on pass 1's resolved width. Two synthetic
// leaves whose height is a non-linear step function of their resolved width lay
// out through the split pipeline: pass 1 resolves the x-edges (the widths), each
// width is projected as a signal into the leaf's getVerticalConstraints(), and
// pass 2 solves the y-edges with height == f(width). The two (width, height)
// pairs the step produces are not colinear through the origin, so no single
// linear relation a combined x+y solve could hold reproduces both -- the height
// can only come from pass 2 reading pass 1's concrete width.
TEST(PureSolverLayout, verticalPassSeesPass1ResolvedWidth)
{
    BoxVariables p;
    BoxVariables q;

    BoxDescriptor dp = stepHeightLeaf(p, 120.0f);
    BoxDescriptor dq = stepHeightLeaf(q, 60.0f);

    // Pass 1: solve the x-edges alone.
    auto horizontalSpec = merge(dp.getHorizontalConstraints(),
            dq.getHorizontalConstraints())
        .map([p, q](std::vector<arrange::Constraint> const& cp,
                    std::vector<arrange::Constraint> const& cq)
            {
                return makeFragment(cp, cq, { p.left, p.right, q.left, q.right });
            });

    auto horizontalSolution =
        layoutRegion(oneFragment(AnySignal<LayoutSpec>(std::move(horizontalSpec))))
        .share();

    // Project each leaf's pass-1 resolved width (right - left) as a signal.
    auto widthP = horizontalSolution.clone().map(
            [p](LayoutSolution const& s) { return readObb(s, p).getSize()[0]; });
    auto widthQ = horizontalSolution.clone().map(
            [q](LayoutSolution const& s) { return readObb(s, q).getSize()[0]; });

    // Pass 2: stage each leaf's vertical constraints on its resolved width and
    // solve the y-edges alone.
    auto verticalSpec = merge(dp.getVerticalConstraints(std::move(widthP)),
            dq.getVerticalConstraints(std::move(widthQ)))
        .map([p, q](std::vector<arrange::Constraint> const& cp,
                    std::vector<arrange::Constraint> const& cq)
            {
                return makeFragment(cp, cq, { p.top, p.bottom, q.top, q.bottom });
            });

    auto verticalSolution =
        layoutRegion(oneFragment(AnySignal<LayoutSpec>(std::move(verticalSpec))));

    auto solutionSignal = combineSolutions(horizontalSolution.clone(),
            std::move(verticalSolution));

    auto context = makeSignalContext(std::move(solutionSignal));
    LayoutSolution solution = context.evaluate<0>().get<0>();

    avg::Obb pObb = readObb(solution, p);
    avg::Obb qObb = readObb(solution, q);

    // 120 -> 30*ceil(120/50) = 30*3 = 90 (ratio 0.75);
    // 60  -> 30*ceil(60/50)  = 30*2 = 60 (ratio 1.0).
    EXPECT_FLOAT_EQ(120.0f, pObb.getSize()[0]);
    EXPECT_FLOAT_EQ(90.0f, pObb.getSize()[1]);
    EXPECT_FLOAT_EQ(60.0f, qObb.getSize()[0]);
    EXPECT_FLOAT_EQ(60.0f, qObb.getSize()[1]);
}
