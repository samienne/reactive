#include "widget/constraintbox.h"
#include "widget/constraintlayout.h"

#include <bqui/modifier/constraintsize.h>
#include <bqui/modifier/instancemodifier.h>
#include <bqui/modifier/setsizehint.h>
#include <bqui/modifier/widgetmodifier.h>

#include <bqui/widget/filler.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/label.h>
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

// Tags a widget's realised instance with an InputArea keyed by @p id, so its
// window-space geometry can be read back through the accumulated transform.
AnyWidget withArea(AnyWidget widget, btl::UniqueId id)
{
    return std::move(widget)
        | modifier::makeWidgetModifier(modifier::makeInstanceModifier(
                    [](Instance instance, btl::UniqueId id)
                    {
                        auto areas = instance.getInputAreas();
                        areas.push_back(makeInputArea(id, instance.getObb()));

                        return std::move(instance)
                            .setInputAreas(std::move(areas));
                    }, constant(id)));
}

// A leaf that carries the weak 100 default a pure-solver leaf owns and tags its
// instance for read-back. The pure-solver path ignores the size hint entirely,
// so a probe's band is set deliberately away from the weak 100 default to prove
// the band is never read.
AnyWidget probe(btl::UniqueId id, Band width, Band height)
{
    return withArea(makeWidget()
            | modifier::setSizeHint(
                constant(SizeHint(simpleSizeHint(width, height))))
            | modifier::defaultSize(),
            id);
}

// A pure-solver filler tagged for geometry read-back.
AnyWidget fillerProbe(btl::UniqueId id)
{
    return withArea(filler(), id);
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

// A single evaluate with NO update passes. The forward-only pure solver settles
// on the first frame -- its constraints ride the builders and its solution is a
// build argument, not a tee'd input -- so the geometry read here has had no
// chance to settle behind.
Instance realiseOnce(AnyWidget widget, avg::Vector2f size)
{
    auto instanceSignal = std::move(widget)(BuildParams())(constant(size))
        .getInstance();

    auto context = makeSignalContext(std::move(instanceSignal));
    return context.evaluate<0>().get<0>();
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

// Form row: a fixed-width label and a filler that takes the rest. The label's
// fixedWidth (strong) holds at 120; the filler carries no default on the layout
// axis, so the container's gap drive pulls it out to close the row. In a
// 400-wide row: 120 + 280.
TEST(PureSolverLayout, formRowFixedLabelAndFiller)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idLabel = btl::makeUniqueId();
    btl::UniqueId const idField = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(idLabel, fixed40, fixed40) | modifier::fixedWidth(120.0f));
    row.push_back(fillerProbe(idField));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    Geometry label = readProbe(instance, idLabel);
    Geometry field = readProbe(instance, idField);

    EXPECT_FLOAT_EQ(120.0f, label.size[0]);
    EXPECT_FLOAT_EQ(280.0f, field.size[0]);

    EXPECT_FLOAT_EQ(0.0f, label.position[0]);
    EXPECT_FLOAT_EQ(120.0f, field.position[0]);
}

// Toolbar: three fixed-width buttons and a filler that pushes them apart. The
// three exact widths (strong) hold; the filler absorbs the slack. In a 400-wide
// row: 80 + 80 + 160 + 80.
TEST(PureSolverLayout, toolbarFixedItemsAndFiller)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();
    btl::UniqueId const idSpacer = btl::makeUniqueId();
    btl::UniqueId const idC = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> items;
    items.push_back(probe(idA, fixed40, fixed40) | modifier::fixedWidth(80.0f));
    items.push_back(probe(idB, fixed40, fixed40) | modifier::fixedWidth(80.0f));
    items.push_back(fillerProbe(idSpacer));
    items.push_back(probe(idC, fixed40, fixed40) | modifier::fixedWidth(80.0f));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(items)))),
            window);

    Geometry a = readProbe(instance, idA);
    Geometry b = readProbe(instance, idB);
    Geometry spacer = readProbe(instance, idSpacer);
    Geometry c = readProbe(instance, idC);

    EXPECT_FLOAT_EQ(80.0f, a.size[0]);
    EXPECT_FLOAT_EQ(80.0f, b.size[0]);
    EXPECT_FLOAT_EQ(160.0f, spacer.size[0]);
    EXPECT_FLOAT_EQ(80.0f, c.size[0]);

    EXPECT_FLOAT_EQ(0.0f, a.position[0]);
    EXPECT_FLOAT_EQ(80.0f, b.position[0]);
    EXPECT_FLOAT_EQ(160.0f, spacer.position[0]);
    EXPECT_FLOAT_EQ(320.0f, c.position[0]);
}

// Two fillers split the slack evenly: they couple to the one shared flex
// variable, so a fixed 100 leaf and two fillers in a 400-wide row give
// 100 + 150 + 150.
TEST(PureSolverLayout, twoFillersSplitSlackEvenly)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idFixed = btl::makeUniqueId();
    btl::UniqueId const idFirst = btl::makeUniqueId();
    btl::UniqueId const idSecond = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(idFixed, fixed40, fixed40) | modifier::fixedWidth(100.0f));
    row.push_back(fillerProbe(idFirst));
    row.push_back(fillerProbe(idSecond));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    EXPECT_FLOAT_EQ(100.0f, readProbe(instance, idFixed).size[0]);
    EXPECT_FLOAT_EQ(150.0f, readProbe(instance, idFirst).size[0]);
    EXPECT_FLOAT_EQ(150.0f, readProbe(instance, idSecond).size[0]);
}

// No implicit fill: with no filler present the last child is not stretched. Two
// fixed 80s and a plain leaf in a 400-wide row leave the plain leaf at its weak
// 100 default and open a trailing gap: 80 + 80 + 100 (140 of slack unfilled).
TEST(PureSolverLayout, plainLastChildIsNotStretched)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();
    btl::UniqueId const idPlain = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(idA, fixed40, fixed40) | modifier::fixedWidth(80.0f));
    row.push_back(probe(idB, fixed40, fixed40) | modifier::fixedWidth(80.0f));
    row.push_back(probe(idPlain, fixed40, fixed40));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    EXPECT_FLOAT_EQ(80.0f, readProbe(instance, idA).size[0]);
    EXPECT_FLOAT_EQ(80.0f, readProbe(instance, idB).size[0]);
    EXPECT_FLOAT_EQ(100.0f, readProbe(instance, idPlain).size[0]);
}

// A capped filler hands its surplus to the others. Three fillers share the
// slack, but the middle one is capped at 60 (required maxWidth); the other two
// absorb what it cannot, so in a 400-wide row: 170 + 60 + 170.
TEST(PureSolverLayout, cappedFillerHandsSurplusToOthers)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idFirst = btl::makeUniqueId();
    btl::UniqueId const idCapped = btl::makeUniqueId();
    btl::UniqueId const idLast = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(fillerProbe(idFirst));
    row.push_back(withArea(filler() | modifier::maxWidth(60.0f), idCapped));
    row.push_back(fillerProbe(idLast));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    EXPECT_FLOAT_EQ(60.0f, readProbe(instance, idCapped).size[0]);
    EXPECT_FLOAT_EQ(170.0f, readProbe(instance, idFirst).size[0]);
    EXPECT_FLOAT_EQ(170.0f, readProbe(instance, idLast).size[0]);
}

// Overflow, never squeeze: two fixed 300s in a 400-wide row keep their sizes and
// overflow past the container's end. The signed trailing gap goes negative
// rather than the children being squeezed to fit.
TEST(PureSolverLayout, fixedChildrenOverflowRatherThanSqueeze)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(idA, fixed40, fixed40) | modifier::fixedWidth(300.0f));
    row.push_back(probe(idB, fixed40, fixed40) | modifier::fixedWidth(300.0f));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    EXPECT_FLOAT_EQ(300.0f, readProbe(instance, idA).size[0]);
    EXPECT_FLOAT_EQ(300.0f, readProbe(instance, idB).size[0]);

    EXPECT_FLOAT_EQ(0.0f, readProbe(instance, idA).position[0]);
    EXPECT_FLOAT_EQ(300.0f, readProbe(instance, idB).position[0]);
}

// The strong exact size and the required bounds reach the right axis: a single
// leaf pins its height to 150 (strong, beating the weak 100 default) and caps
// its width at 60 (required, holding the default 100 width down). fixedHeight
// feeds the vertical solve, maxWidth the horizontal one.
TEST(PureSolverLayout, exactAndBoundedLeafOverridesDefaults)
{
    avg::Vector2f const window(200.0f, 300.0f);

    btl::UniqueId const id = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> children;
    children.push_back(probe(id, fixed40, fixed40)
            | modifier::fixedHeight(150.0f)
            | modifier::maxWidth(60.0f));

    Instance instance = realiseConverged(
            pureSolverRoot(vbox(ArraySignal<AnyWidget>(std::move(children)))),
            window);

    Geometry g = readProbe(instance, id);

    EXPECT_FLOAT_EQ(60.0f, g.size[0]);
    EXPECT_FLOAT_EQ(150.0f, g.size[1]);
    EXPECT_FLOAT_EQ(0.0f, g.position[0]);
}

// A shipped content leaf carries its own weak 100 default with no defaultSize()
// at the call site: a plain label() beside a filler settles at 100 and the
// filler takes the rest, so baking the default into the leaf factories works.
TEST(PureSolverLayout, shippedLeafCarriesItsOwnDefault)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idLabel = btl::makeUniqueId();
    btl::UniqueId const idFiller = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(withArea(label(std::string("hi")), idLabel));
    row.push_back(fillerProbe(idFiller));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    EXPECT_FLOAT_EQ(100.0f, readProbe(instance, idLabel).size[0]);
    EXPECT_FLOAT_EQ(300.0f, readProbe(instance, idFiller).size[0]);
}

// maxWidth alone caps the weak 100 default: a required upper bound below the
// default holds the width down to 60.
TEST(PureSolverLayout, maxWidthAloneCapsTheDefault)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const id = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(id, fixed40, fixed40) | modifier::maxWidth(60.0f));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    EXPECT_FLOAT_EQ(60.0f, readProbe(instance, id).size[0]);
}

// minWidth alone raises the weak 100 default: a required lower bound above the
// default lifts the width to 200 in a row with room to spare.
TEST(PureSolverLayout, minWidthAloneRaisesTheDefault)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const id = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(id, fixed40, fixed40) | modifier::minWidth(200.0f));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    EXPECT_FLOAT_EQ(200.0f, readProbe(instance, id).size[0]);
}

// The over-constraining edge: a required minWidth larger than the whole row.
// The child needs 200 in a 100-wide window. This asserts the solver's ACTUAL
// behavior (filled in from the run), including what happens to a fixed-width
// sibling, so the required-vs-strong choice can be judged.
TEST(PureSolverLayout, minWidthLargerThanRow)
{
    avg::Vector2f const window(100.0f, 100.0f);

    btl::UniqueId const idBig = btl::makeUniqueId();
    btl::UniqueId const idSibling = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(idBig, fixed40, fixed40) | modifier::minWidth(200.0f));
    row.push_back(probe(idSibling, fixed40, fixed40)
            | modifier::fixedWidth(50.0f));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    Geometry big = readProbe(instance, idBig);
    Geometry sibling = readProbe(instance, idSibling);

    // The required min holds at 200 and overflows the row; the signed gap goes
    // negative and the fixed sibling keeps its size, pushed past the row's end.
    EXPECT_FLOAT_EQ(200.0f, big.size[0]);
    EXPECT_FLOAT_EQ(0.0f, big.position[0]);
    EXPECT_FLOAT_EQ(50.0f, sibling.size[0]);
    EXPECT_FLOAT_EQ(200.0f, sibling.position[0]);
}

// The settle-behind is gone: the nested vbox({hbox, hbox}) demo is correct on a
// SINGLE evaluate<0>() with no update passes. Rows stack, fixed items hold their
// widths, each nested row gets a determinate height, and every filler is
// positive and fills its row's slack. Before the forward-only restructure this
// was fully bunched at the origin on a single evaluate.
TEST(PureSolverLayout, nestedDemoCorrectOnSingleEvaluate)
{
    avg::Vector2f const window(400.0f, 200.0f);

    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idFill1 = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();
    btl::UniqueId const idC = btl::makeUniqueId();
    btl::UniqueId const idFill2 = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row1;
    row1.push_back(probe(idA, fixed40, fixed40) | modifier::fixedWidth(80.0f));
    row1.push_back(fillerProbe(idFill1));
    row1.push_back(probe(idB, fixed40, fixed40) | modifier::fixedWidth(80.0f));

    std::vector<ArraySignal<AnyWidget>> row2;
    row2.push_back(probe(idC, fixed40, fixed40) | modifier::fixedWidth(120.0f));
    row2.push_back(fillerProbe(idFill2));

    std::vector<ArraySignal<AnyWidget>> rows;
    rows.push_back(hbox(ArraySignal<AnyWidget>(std::move(row1))));
    rows.push_back(hbox(ArraySignal<AnyWidget>(std::move(row2))));

    Instance instance = realiseOnce(
            pureSolverRoot(vbox(ArraySignal<AnyWidget>(std::move(rows)))),
            window);

    Geometry a = readProbe(instance, idA);
    Geometry fill1 = readProbe(instance, idFill1);
    Geometry b = readProbe(instance, idB);
    Geometry c = readProbe(instance, idC);
    Geometry fill2 = readProbe(instance, idFill2);

    // Fixed items hold; each filler is positive and takes its row's slack.
    EXPECT_FLOAT_EQ(80.0f, a.size[0]);
    EXPECT_FLOAT_EQ(240.0f, fill1.size[0]);
    EXPECT_FLOAT_EQ(80.0f, b.size[0]);
    EXPECT_FLOAT_EQ(120.0f, c.size[0]);
    EXPECT_FLOAT_EQ(280.0f, fill2.size[0]);

    // Each row is a determinate 100 tall and they stack: row1 on top (y=100 in
    // the y-up window), row2 below (y=0).
    EXPECT_FLOAT_EQ(100.0f, a.size[1]);
    EXPECT_FLOAT_EQ(100.0f, c.size[1]);
    EXPECT_GT(fill1.size[1], 0.0f);
    EXPECT_GT(fill2.size[1], 0.0f);
    EXPECT_FLOAT_EQ(100.0f, a.position[1]);
    EXPECT_FLOAT_EQ(0.0f, c.position[1]);
}
