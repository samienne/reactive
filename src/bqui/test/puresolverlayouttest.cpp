#include "widget/constraintbox.h"
#include "widget/constraintlayout.h"

#include <bqui/modifier/constraintsize.h>
#include <bqui/modifier/frame.h>
#include <bqui/modifier/instancemodifier.h>
#include <bqui/modifier/margin.h>
#include <bqui/modifier/onclick.h>
#include <bqui/modifier/setminimumsize.h>
#include <bqui/modifier/setsize.h>
#include <bqui/modifier/setsizehint.h>
#include <bqui/modifier/widgetmodifier.h>

#include <bqui/widget/filler.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/label.h>
#include <bqui/widget/vbox.h>
#include <bqui/widget/widget.h>

#include <bqui/shape/rectangle.h>

#include <bqui/buildparams.h>
#include <bqui/inputarea.h>
#include <bqui/simplesizehint.h>
#include <bqui/sizehint.h>

#include <avg/brush.h>
#include <avg/color.h>

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

// A content leaf: its SizeHint band is bridged into the pure natural by
// defaultSize(), so a probe sizes to its SizeHint's natural on each axis unless
// an explicit size word overrides it. Tagged for geometry read-back.
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

// A SizeHint whose natural height is inversely proportional to width -- narrower
// means taller (height == area / width) -- so it reflows only when phase 2 reads
// the resolved width rather than the natural width.
struct ReflowHint
{
    float area;
    float fallbackWidth;

    AxisHint getWidth() const
    {
        return AxisHint{ Band{ fallbackWidth, fallbackWidth, fallbackWidth },
                Anchors() };
    }

    AxisHint getHeightForWidth(float width) const
    {
        float height = area / width;
        return AxisHint{ Band{ height, height, height }, Anchors() };
    }

    AxisHint getWidthForHeight(float) const
    {
        return getWidth();
    }
};

// A content leaf carrying a width-dependent height, filling its row so its
// resolved width tracks the window. Tagged for geometry read-back.
AnyWidget reflowProbe(btl::UniqueId id, float area, float fallbackWidth)
{
    return withArea(makeWidget()
            | modifier::setSizeHint(
                constant(SizeHint(ReflowHint{ area, fallbackWidth })))
            | modifier::defaultSize()
            | modifier::fill(),
            id);
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

// A size-dependent modifier must not orphan a pure-solver constraint. A
// fixedWidth(80) leaf wrapped in onClick, which mints a fresh builder through
// the with-size path, keeps its width at 80 rather than falling back to the
// weak 100 default: the with-size path carries the builder's box, guide
// alignments and pure-solver constraints onto the builder it mints.
TEST(PureSolverLayout, withSizeModifierPreservesConstraint)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const id = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(id, fixed40, fixed40)
            | modifier::fixedWidth(80.0f)
            | modifier::onClick(1, [](ClickEvent const&) {}));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    EXPECT_FLOAT_EQ(80.0f, readProbe(instance, id).size[0]);
}

// A real nested vbox behind pureSolverRoot lays its content leaves out at their
// SizeHint natural: every leaf carries a 40x40 band and comes out 40x40, stacked
// edge to edge. The outer column holds a leaf and the inner column; the inner
// holds two leaves. Nothing forces the leaves wider, so they size to content on
// both axes (a leaf fills only when a container has slack to give -- here it
// does not), and stack from the top with the leftover as a trailing gap.
TEST(PureSolverLayout, nestedColumnsSizeToContent)
{
    avg::Vector2f const window(100.0f, 300.0f);

    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idC = btl::makeUniqueId();
    btl::UniqueId const idD = btl::makeUniqueId();

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

    // Every leaf sizes to its 40x40 content band, left-aligned in the column.
    for (Geometry const& g : { a, c, d })
    {
        EXPECT_FLOAT_EQ(40.0f, g.size[0]);
        EXPECT_FLOAT_EQ(40.0f, g.size[1]);
        EXPECT_FLOAT_EQ(0.0f, g.position[0]);
    }

    // Stacked from the top of the 300-tall window (y-up): A at 260..300, then C
    // at 220..260, then D at 180..220, the remaining 180 an unfilled gap below.
    EXPECT_FLOAT_EQ(260.0f, a.position[1]);
    EXPECT_FLOAT_EQ(220.0f, c.position[1]);
    EXPECT_FLOAT_EQ(180.0f, d.position[1]);
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
// fixed 80s and a plain content leaf in a 400-wide row leave the plain leaf at
// its 40 content width and open a trailing gap: 80 + 80 + 40 (200 unfilled).
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
    EXPECT_FLOAT_EQ(40.0f, readProbe(instance, idPlain).size[0]);
}

// A capped filler hands its surplus to the others. Three fillers share the
// slack, but the middle one is capped at 60 (strong maxWidth, which beats the
// weakest flex coupling); the other two absorb what it cannot, so in a 400-wide
// row: 170 + 60 + 170.
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

// The strong exact size and the strong bounds reach the right axis: a single
// leaf pins its height to 150 (strong, beating its content height) and caps its
// width at 60 (strong, beating the weak content pull holding its 100 content
// width). fixedHeight feeds the vertical solve, maxWidth the horizontal one.
TEST(PureSolverLayout, exactAndBoundedLeafOverridesDefaults)
{
    avg::Vector2f const window(200.0f, 300.0f);

    btl::UniqueId const id = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> children;
    children.push_back(probe(id, fixed100, fixed100)
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

// A shipped content leaf sizes to its own content with no defaultSize() at the
// call site: a plain label() beside a filler settles at its measured content
// width and the filler takes the rest of the row, so the SizeHint bridge reaches
// the shipped leaf factories. The exact width is font-dependent, so this asserts
// the invariant -- the label is content-sized (positive, well under the full
// row) and the filler absorbs the remainder -- not a pixel count.
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

    Geometry labelG = readProbe(instance, idLabel);
    Geometry fillerG = readProbe(instance, idFiller);

    EXPECT_GT(labelG.size[0], 0.0f);
    EXPECT_LT(labelG.size[0], 400.0f);
    EXPECT_GT(fillerG.size[0], 0.0f);
    EXPECT_FLOAT_EQ(400.0f, labelG.size[0] + fillerG.size[0]);
    EXPECT_FLOAT_EQ(0.0f, labelG.position[0]);
    EXPECT_FLOAT_EQ(labelG.size[0], fillerG.position[0]);
}

// maxWidth alone caps the content width: a strong upper bound below the leaf's
// 100 content width beats the weak content pull and holds it down to 60.
TEST(PureSolverLayout, maxWidthAloneCapsTheDefault)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const id = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(id, fixed100, fixed100) | modifier::maxWidth(60.0f));

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

// The over-constraining edge: a strong minWidth larger than the whole row. The
// child needs 200 in a 100-wide window. With no required conflict present the
// strong min behaves exactly as a required one would -- it holds at 200 and
// overflows the row -- and, crucially, the fixed-width sibling still lays out.
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

    // The strong min holds at 200 and overflows the row; the signed gap goes
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

    // Each row is a determinate 40 tall (its content-sized items) and they stack
    // from the top: row1 at 160..200 in the y-up window, row2 at 120..160.
    EXPECT_FLOAT_EQ(40.0f, a.size[1]);
    EXPECT_FLOAT_EQ(40.0f, c.size[1]);
    EXPECT_GT(fill1.size[1], 0.0f);
    EXPECT_GT(fill2.size[1], 0.0f);
    EXPECT_FLOAT_EQ(160.0f, a.position[1]);
    EXPECT_FLOAT_EQ(120.0f, c.position[1]);
}

// The load-bearing invariant: exactly one band on the current outermost box, and
// every wrapper subsumes the inner band. A single margin around a fixed size
// insets the content: fixedSize(100) sets the outer band, the wrapper ties the
// inner box 10 in from it, so the image settles at 100 - 2*10 = 80. The leaf
// sits inside a vbox in a 300-wide window, so its size comes from the solved
// band (fixedSize 100), not from the window filling it.
TEST(PureSolverLayout, singleMarginInsetsFixedSize)
{
    avg::Vector2f const window(300.0f, 300.0f);

    btl::UniqueId const id = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> children;
    children.push_back(probe(id, fixed40, fixed40)
            | modifier::margin(10.0f)
            | modifier::fixedSize(avg::Vector2f(100.0f, 100.0f)));

    Instance instance = realiseOnce(
            pureSolverRoot(vbox(ArraySignal<AnyWidget>(std::move(children)))),
            window);

    Geometry g = readProbe(instance, id);
    EXPECT_FLOAT_EQ(80.0f, g.size[0]);
    EXPECT_FLOAT_EQ(80.0f, g.size[1]);
}

// The nested-margin worked example, on a single evaluate. Stacking
// margin | size | margin | size subsumes the inner band at each wrapper: the
// outer fixedSize(100) is the one band, and the two insets distribute it inward
// to image = 100 - 2*(10 + 10) = 60. Were a wrapper to keep its inner band
// instead of subsuming it, the later size would contradict the stale constraint
// and the number would not fall out. A single evaluate<0>() proves the
// forward-only pure path settles it in one pass.
TEST(PureSolverLayout, nestedMarginsSubsumeInnerBands)
{
    avg::Vector2f const window(300.0f, 300.0f);

    btl::UniqueId const id = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> children;
    children.push_back(probe(id, fixed40, fixed40)
            | modifier::margin(10.0f)
            | modifier::fixedSize(avg::Vector2f(100.0f, 100.0f))
            | modifier::margin(10.0f)
            | modifier::fixedSize(avg::Vector2f(100.0f, 100.0f)));

    Instance instance = realiseOnce(
            pureSolverRoot(vbox(ArraySignal<AnyWidget>(std::move(children)))),
            window);

    Geometry g = readProbe(instance, id);
    EXPECT_FLOAT_EQ(60.0f, g.size[0]);
    EXPECT_FLOAT_EQ(60.0f, g.size[1]);
}

// Flex aggregates up: an hbox holding a filler has flex>0 on its main axis and
// so is itself a filler to its parent. An outer row of a fixed 100 leaf and such
// an inner hbox in a 400-wide window: the inner stretches to absorb the outer's
// 300 of leftover (behaving as a filler), and inside it its own fixed child
// stays 60 while its filler takes the 240 of inner slack. This exercises both
// the flex summary riding up and the shared-F coupling reaching the inner.
TEST(PureSolverLayout, fillerInHboxIsAFiller)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idFixed = btl::makeUniqueId();
    btl::UniqueId const idInnerFixed = btl::makeUniqueId();
    btl::UniqueId const idInnerFiller = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> innerRow;
    innerRow.push_back(probe(idInnerFixed, fixed40, fixed40)
            | modifier::fixedWidth(60.0f));
    innerRow.push_back(fillerProbe(idInnerFiller));
    AnyWidget inner = hbox(ArraySignal<AnyWidget>(std::move(innerRow)));

    std::vector<ArraySignal<AnyWidget>> outerRow;
    outerRow.push_back(probe(idFixed, fixed40, fixed40)
            | modifier::fixedWidth(100.0f));
    outerRow.push_back(std::move(inner));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(outerRow)))),
            window);

    Geometry fixed = readProbe(instance, idFixed);
    Geometry innerFixed = readProbe(instance, idInnerFixed);
    Geometry innerFiller = readProbe(instance, idInnerFiller);

    // The outer's fixed child holds 100; the inner hbox behaved as a filler.
    EXPECT_FLOAT_EQ(100.0f, fixed.size[0]);
    EXPECT_FLOAT_EQ(0.0f, fixed.position[0]);

    // Inside the stretched inner (spanning 100..400): its fixed child stays 60
    // and its filler takes the inner's slack, 300 - 60 = 240.
    EXPECT_FLOAT_EQ(60.0f, innerFixed.size[0]);
    EXPECT_FLOAT_EQ(100.0f, innerFixed.position[0]);
    EXPECT_FLOAT_EQ(240.0f, innerFiller.size[0]);
    EXPECT_FLOAT_EQ(160.0f, innerFiller.position[0]);
}

// A flexing container couples to its parent by its aggregated flex weight, not a
// flat 1. An inner hbox of two fillers aggregates flex coeff 2, so beside a plain
// filler (coeff 1) in a 300-wide row the parent splits the slack 2:1 -- the inner
// gets 200, the outer filler 100. Weight propagation makes the nesting behave as
// three leaf fillers sharing evenly: all three end at 100. A coeff-1 coupling (the
// bug) would instead give the inner 150 (two 75s) and the outer filler 150.
TEST(PureSolverLayout, weightedContainerFlexSplitsByAggregatedWeight)
{
    avg::Vector2f const window(300.0f, 100.0f);

    btl::UniqueId const idInnerA = btl::makeUniqueId();
    btl::UniqueId const idInnerB = btl::makeUniqueId();
    btl::UniqueId const idOuter = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> innerRow;
    innerRow.push_back(fillerProbe(idInnerA));
    innerRow.push_back(fillerProbe(idInnerB));
    AnyWidget inner = hbox(ArraySignal<AnyWidget>(std::move(innerRow)));

    std::vector<ArraySignal<AnyWidget>> outerRow;
    outerRow.push_back(std::move(inner));
    outerRow.push_back(fillerProbe(idOuter));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(outerRow)))),
            window);

    Geometry innerA = readProbe(instance, idInnerA);
    Geometry innerB = readProbe(instance, idInnerB);
    Geometry outer = readProbe(instance, idOuter);

    // The inner hbox (coeff 2) takes 200, split into two 100s; the outer filler
    // (coeff 1) takes 100 -- a 2:1 split, and all three fillers land at 100.
    EXPECT_FLOAT_EQ(100.0f, innerA.size[0]);
    EXPECT_FLOAT_EQ(100.0f, innerB.size[0]);
    EXPECT_FLOAT_EQ(100.0f, outer.size[0]);
    EXPECT_FLOAT_EQ(0.0f, innerA.position[0]);
    EXPECT_FLOAT_EQ(100.0f, innerB.position[0]);
    EXPECT_FLOAT_EQ(200.0f, outer.position[0]);
}

// Overflow versus flex-response, forced by a too-small window. Fixed children
// keep their size and overflow (the signed trailing gap goes negative); the same
// shape with fillers shrinks them to share the row. Two 100s in a 150 row on the
// one hand, two fillers on the other.
TEST(PureSolverLayout, overflowVersusFlexResponse)
{
    avg::Vector2f const window(150.0f, 100.0f);

    // Fixed children overflow rather than squeeze.
    {
        btl::UniqueId const idA = btl::makeUniqueId();
        btl::UniqueId const idB = btl::makeUniqueId();

        std::vector<ArraySignal<AnyWidget>> row;
        row.push_back(probe(idA, fixed40, fixed40)
                | modifier::fixedWidth(100.0f));
        row.push_back(probe(idB, fixed40, fixed40)
                | modifier::fixedWidth(100.0f));

        Instance instance = realiseConverged(
                pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
                window);

        Geometry a = readProbe(instance, idA);
        Geometry b = readProbe(instance, idB);

        EXPECT_FLOAT_EQ(100.0f, a.size[0]);
        EXPECT_FLOAT_EQ(100.0f, b.size[0]);
        // The second child runs 100..200, past the row's 150 end: it overflows,
        // the signed gap negative, rather than being squeezed.
        EXPECT_FLOAT_EQ(0.0f, a.position[0]);
        EXPECT_FLOAT_EQ(100.0f, b.position[0]);
    }

    // Flexible children respond: they shrink to share the row.
    {
        btl::UniqueId const idA = btl::makeUniqueId();
        btl::UniqueId const idB = btl::makeUniqueId();

        std::vector<ArraySignal<AnyWidget>> row;
        row.push_back(fillerProbe(idA));
        row.push_back(fillerProbe(idB));

        Instance instance = realiseConverged(
                pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
                window);

        EXPECT_FLOAT_EQ(75.0f, readProbe(instance, idA).size[0]);
        EXPECT_FLOAT_EQ(75.0f, readProbe(instance, idB).size[0]);
    }
}

// A content leaf sizes to its SizeHint natural, not a flat default. A probe whose
// natural is a distinct 137x24 comes out exactly 137x24 -- not the old flat 100,
// not the 40 the other probes carry -- proving the SizeHint's natural now drives
// the pure band on both axes.
TEST(PureSolverLayout, leafSizesToContent)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const id = btl::makeUniqueId();

    Band const content137 = { 137.0f, 137.0f, 137.0f };
    Band const content24 = { 24.0f, 24.0f, 24.0f };

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(id, content137, content24));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    Geometry g = readProbe(instance, id);
    EXPECT_FLOAT_EQ(137.0f, g.size[0]);
    EXPECT_FLOAT_EQ(24.0f, g.size[1]);
}

// Content leaves take their content widths and a filler absorbs the rest. Two
// probes of distinct content widths (137 and 63) and a filler in a 400-wide row:
// the probes settle at 137 and 63, and the filler takes the remaining 200.
TEST(PureSolverLayout, contentLeavesShareRowWithFiller)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idWide = btl::makeUniqueId();
    btl::UniqueId const idNarrow = btl::makeUniqueId();
    btl::UniqueId const idFiller = btl::makeUniqueId();

    Band const content137 = { 137.0f, 137.0f, 137.0f };
    Band const content63 = { 63.0f, 63.0f, 63.0f };
    Band const content24 = { 24.0f, 24.0f, 24.0f };

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(idWide, content137, content24));
    row.push_back(probe(idNarrow, content63, content24));
    row.push_back(fillerProbe(idFiller));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    Geometry wide = readProbe(instance, idWide);
    Geometry narrow = readProbe(instance, idNarrow);
    Geometry filler = readProbe(instance, idFiller);

    EXPECT_FLOAT_EQ(137.0f, wide.size[0]);
    EXPECT_FLOAT_EQ(63.0f, narrow.size[0]);
    EXPECT_FLOAT_EQ(200.0f, filler.size[0]);

    EXPECT_FLOAT_EQ(0.0f, wide.position[0]);
    EXPECT_FLOAT_EQ(137.0f, narrow.position[0]);
    EXPECT_FLOAT_EQ(200.0f, filler.position[0]);
}

// fill() makes an ordinary content widget behave like a filler. A fixed 100 leaf
// and a content probe carrying fill() in a 400-wide row: the fixed leaf holds
// 100 and the filled probe absorbs the 300 of slack, its 40 content width
// dropped under the distribution.
TEST(PureSolverLayout, fillModifierActsAsFiller)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idFixed = btl::makeUniqueId();
    btl::UniqueId const idFilled = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(idFixed, fixed40, fixed40) | modifier::fixedWidth(100.0f));
    row.push_back(probe(idFilled, fixed40, fixed40) | modifier::fill());

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    Geometry fixed = readProbe(instance, idFixed);
    Geometry filled = readProbe(instance, idFilled);

    EXPECT_FLOAT_EQ(100.0f, fixed.size[0]);
    EXPECT_FLOAT_EQ(300.0f, filled.size[0]);
    EXPECT_FLOAT_EQ(0.0f, fixed.position[0]);
    EXPECT_FLOAT_EQ(100.0f, filled.position[0]);
}

// Two fill() widgets split the slack evenly, coupling to the one shared flex
// variable just as two fillers do: a fixed 100 leaf and two filled probes in a
// 400-wide row give 100 + 150 + 150.
TEST(PureSolverLayout, twoFillModifiersShareSlackEvenly)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idFixed = btl::makeUniqueId();
    btl::UniqueId const idFirst = btl::makeUniqueId();
    btl::UniqueId const idSecond = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(idFixed, fixed40, fixed40) | modifier::fixedWidth(100.0f));
    row.push_back(probe(idFirst, fixed40, fixed40) | modifier::fill());
    row.push_back(probe(idSecond, fixed40, fixed40) | modifier::fill());

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    EXPECT_FLOAT_EQ(100.0f, readProbe(instance, idFixed).size[0]);
    EXPECT_FLOAT_EQ(150.0f, readProbe(instance, idFirst).size[0]);
    EXPECT_FLOAT_EQ(150.0f, readProbe(instance, idSecond).size[0]);
}

// grow(weight) splits the slack in proportion to the weights: a grow(1) and a
// grow(3) probe in a 400-wide row take 100 and 300 (a 1:3 share).
TEST(PureSolverLayout, growWeightsSplitSlackByRatio)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idOne = btl::makeUniqueId();
    btl::UniqueId const idThree = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(idOne, fixed40, fixed40) | modifier::grow(1.0f));
    row.push_back(probe(idThree, fixed40, fixed40) | modifier::grow(3.0f));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    EXPECT_FLOAT_EQ(100.0f, readProbe(instance, idOne).size[0]);
    EXPECT_FLOAT_EQ(300.0f, readProbe(instance, idThree).size[0]);
    EXPECT_FLOAT_EQ(0.0f, readProbe(instance, idOne).position[0]);
    EXPECT_FLOAT_EQ(100.0f, readProbe(instance, idThree).position[0]);
}

// A bare shape sizes to a modest pure default, not its banded fill-everything
// hint: a plain filled rectangle in a wide window comes out 100x100 rather than
// the absurd ~10000 its SizeHint natural would give. A fixedSize still overrides
// it -- the swatch path is unbroken.
TEST(PureSolverLayout, bareShapeHasModestPureDefault)
{
    avg::Vector2f const window(400.0f, 400.0f);

    btl::UniqueId const idBare = btl::makeUniqueId();
    btl::UniqueId const idSized = btl::makeUniqueId();

    avg::Brush const brush{ avg::Color(1.0f, 0.0f, 0.0f, 1.0f) };

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(withArea(shape::rectangle().fill(brush), idBare));
    row.push_back(withArea(shape::rectangle().fill(brush)
                | modifier::fixedSize(avg::Vector2f(48.0f, 48.0f)), idSized));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    Geometry bare = readProbe(instance, idBare);
    Geometry sized = readProbe(instance, idSized);

    EXPECT_FLOAT_EQ(100.0f, bare.size[0]);
    EXPECT_FLOAT_EQ(100.0f, bare.size[1]);
    EXPECT_FLOAT_EQ(48.0f, sized.size[0]);
    EXPECT_FLOAT_EQ(48.0f, sized.size[1]);
}

// The key win of strong over required bounds: a widget whose own bounds
// contradict -- minWidth(200) tied against maxWidth(60) -- resolves to SOME
// determinate size instead of throwing an arrange::Error that solveLayout would
// catch by returning the previous solution, freezing every widget in the region.
// The proof is the sibling: it still lays out at its own fixed 50 (a frozen
// region reads back the empty previous solution, sizing it to 0).
TEST(PureSolverLayout, contradictoryBoundsDoNotFreezeRegion)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idSibling = btl::makeUniqueId();
    btl::UniqueId const idClash = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(idSibling, fixed40, fixed40)
            | modifier::fixedWidth(50.0f));
    row.push_back(probe(idClash, fixed40, fixed40)
            | modifier::minWidth(200.0f)
            | modifier::maxWidth(60.0f));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    Geometry sibling = readProbe(instance, idSibling);
    Geometry clash = readProbe(instance, idClash);

    // The sibling solved normally: the region did not freeze.
    EXPECT_FLOAT_EQ(50.0f, sibling.size[0]);
    EXPECT_FLOAT_EQ(0.0f, sibling.position[0]);

    // The contradictory widget settled at a determinate, finite size somewhere
    // between its tied bounds rather than aborting the solve.
    EXPECT_TRUE(std::isfinite(clash.size[0]));
    EXPECT_GT(clash.size[0], 0.0f);
    EXPECT_FLOAT_EQ(50.0f, clash.position[0]);
}

// Min aggregates as a main-axis SUM up a container. An inner hbox of two leaves,
// each minWidth(100), publishes an aggregate min width of 100 + 100 = 200 (its
// content natural, 40 + 40 = 80, is far weaker). Placed beside a filler in a
// 400-wide row, that strong aggregate min holds the inner hbox at 200 and the
// filler takes the remaining 200 -- where without the aggregation the inner's
// weak natural would win and collapse it, handing the filler 320.
TEST(PureSolverLayout, minAggregatesSumOnMainAxis)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();
    btl::UniqueId const idFiller = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> innerRow;
    innerRow.push_back(probe(idA, fixed40, fixed40) | modifier::minWidth(100.0f));
    innerRow.push_back(probe(idB, fixed40, fixed40) | modifier::minWidth(100.0f));
    AnyWidget inner = hbox(ArraySignal<AnyWidget>(std::move(innerRow)));

    std::vector<ArraySignal<AnyWidget>> outerRow;
    outerRow.push_back(std::move(inner));
    outerRow.push_back(fillerProbe(idFiller));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(outerRow)))),
            window);

    Geometry a = readProbe(instance, idA);
    Geometry b = readProbe(instance, idB);
    Geometry filler = readProbe(instance, idFiller);

    // Each leaf holds its own min; they tile to 200 and the aggregate min holds
    // the inner hbox there, so the filler takes the other 200.
    EXPECT_FLOAT_EQ(100.0f, a.size[0]);
    EXPECT_FLOAT_EQ(100.0f, b.size[0]);
    EXPECT_FLOAT_EQ(0.0f, a.position[0]);
    EXPECT_FLOAT_EQ(100.0f, b.position[0]);
    EXPECT_FLOAT_EQ(200.0f, filler.size[0]);
    EXPECT_FLOAT_EQ(200.0f, filler.position[0]);
}

// Min aggregates as a cross-axis MAX up a container. An inner vbox (layout axis
// vertical) of two leaves with minWidth(150) and minWidth(80) publishes an
// aggregate min width of max(150, 80) = 150 across its cross axis. Beside a
// filler in a 400-wide row, that holds the inner vbox at 150 and the filler
// takes 250 -- without the aggregation the inner's weak natural collapses it and
// the filler takes 360.
TEST(PureSolverLayout, minAggregatesMaxOnCrossAxis)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idWide = btl::makeUniqueId();
    btl::UniqueId const idNarrow = btl::makeUniqueId();
    btl::UniqueId const idFiller = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> innerCol;
    innerCol.push_back(probe(idWide, fixed40, fixed40)
            | modifier::minWidth(150.0f));
    innerCol.push_back(probe(idNarrow, fixed40, fixed40)
            | modifier::minWidth(80.0f));
    AnyWidget inner = vbox(ArraySignal<AnyWidget>(std::move(innerCol)));

    std::vector<ArraySignal<AnyWidget>> outerRow;
    outerRow.push_back(std::move(inner));
    outerRow.push_back(fillerProbe(idFiller));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(outerRow)))),
            window);

    Geometry wide = readProbe(instance, idWide);
    Geometry narrow = readProbe(instance, idNarrow);
    Geometry filler = readProbe(instance, idFiller);

    // Each leaf holds its own min; the widest (150) sets the inner vbox's cross
    // min, so the filler takes 400 - 150 = 250.
    EXPECT_FLOAT_EQ(150.0f, wide.size[0]);
    EXPECT_FLOAT_EQ(80.0f, narrow.size[0]);
    EXPECT_FLOAT_EQ(250.0f, filler.size[0]);
    EXPECT_FLOAT_EQ(150.0f, filler.position[0]);
}

// A frame is layout-transparent in a pure region: its margin insets the
// background shape, not the foreground child, so a framed leaf solves to the
// exact box a bare one does. Before the passthrough fix the framed child lost
// its band and collapsed to the weak 100x100 default at the wrong position.
TEST(PureSolverLayout, frameIsLayoutTransparentInPureRegion)
{
    avg::Vector2f const window(300.0f, 300.0f);

    btl::UniqueId const idBare = btl::makeUniqueId();
    btl::UniqueId const idFramed = btl::makeUniqueId();

    auto column = [](btl::UniqueId id, bool framed) -> AnyWidget
    {
        AnyWidget leaf = framed
            ? (probe(id, fixed40, fixed40) | modifier::frame())
            : probe(id, fixed40, fixed40);
        std::vector<ArraySignal<AnyWidget>> children;
        children.push_back(std::move(leaf));
        return vbox(ArraySignal<AnyWidget>(std::move(children)));
    };

    Instance bare = realiseConverged(
            pureSolverRoot(column(idBare, false)), window);
    Instance framed = realiseConverged(
            pureSolverRoot(column(idFramed, true)), window);

    Geometry b = readProbe(bare, idBare);
    Geometry f = readProbe(framed, idFramed);

    // The framed leaf lands on the same box as the bare one, and that box is the
    // probe's real 40x40 band -- not the collapsed 100x100 default.
    EXPECT_FLOAT_EQ(b.size[0], f.size[0]);
    EXPECT_FLOAT_EQ(b.size[1], f.size[1]);
    EXPECT_FLOAT_EQ(b.position[0], f.position[0]);
    EXPECT_FLOAT_EQ(b.position[1], f.position[1]);
    EXPECT_FLOAT_EQ(40.0f, f.size[0]);
    EXPECT_FLOAT_EQ(40.0f, f.size[1]);
}

// The container aggregates a framed child's real band, not the collapsed
// default: a framed 40-wide leaf beside a fixed 80 leaf tiles to 40 + 80, so the
// second child sits at 40. A lost band would put the framed child at 100 wide
// and push the sibling to 100.
TEST(PureSolverLayout, framedChildAggregatesRealBandInHbox)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idFramed = btl::makeUniqueId();
    btl::UniqueId const idFixed = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(idFramed, fixed40, fixed40) | modifier::frame());
    row.push_back(probe(idFixed, fixed40, fixed40) | modifier::fixedWidth(80.0f));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    Geometry framed = readProbe(instance, idFramed);
    Geometry fixed = readProbe(instance, idFixed);

    EXPECT_FLOAT_EQ(40.0f, framed.size[0]);
    EXPECT_FLOAT_EQ(0.0f, framed.position[0]);
    EXPECT_FLOAT_EQ(80.0f, fixed.size[0]);
    EXPECT_FLOAT_EQ(40.0f, fixed.position[0]);
}

// vfiller flexes vertically in a pure vbox: a 40-tall content leaf above it, and
// the vfiller absorbs the rest of the column (300 - 40 = 260) while pinning its
// width to zero on the cross axis.
TEST(PureSolverLayout, vfillerFlexesInPureVbox)
{
    avg::Vector2f const window(100.0f, 300.0f);

    btl::UniqueId const idFixed = btl::makeUniqueId();
    btl::UniqueId const idFiller = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> column;
    column.push_back(probe(idFixed, fixed40, fixed40));
    column.push_back(withArea(vfiller(), idFiller));

    Instance instance = realiseConverged(
            pureSolverRoot(vbox(ArraySignal<AnyWidget>(std::move(column)))),
            window);

    Geometry fixed = readProbe(instance, idFixed);
    Geometry filler = readProbe(instance, idFiller);

    EXPECT_FLOAT_EQ(40.0f, fixed.size[1]);
    EXPECT_FLOAT_EQ(260.0f, filler.size[1]);
    EXPECT_FLOAT_EQ(0.0f, filler.size[0]);
}

// hfiller flexes horizontally in a pure hbox: a fixed 80-wide leaf beside it, and
// the hfiller takes the rest of the row (400 - 80 = 320) while pinning its height
// to zero on the cross axis.
TEST(PureSolverLayout, hfillerFlexesInPureHbox)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idFixed = btl::makeUniqueId();
    btl::UniqueId const idFiller = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(idFixed, fixed40, fixed40) | modifier::fixedWidth(80.0f));
    row.push_back(withArea(hfiller(), idFiller));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    Geometry fixed = readProbe(instance, idFixed);
    Geometry filler = readProbe(instance, idFiller);

    EXPECT_FLOAT_EQ(80.0f, fixed.size[0]);
    EXPECT_FLOAT_EQ(320.0f, filler.size[0]);
    EXPECT_FLOAT_EQ(0.0f, filler.size[1]);
}

// Directionality: a vfiller placed in a pure HBOX must not steal the row's
// horizontal slack -- it fills only its vertical axis. Beside a fixed 80 leaf in
// a 400 row it stays 0 wide (the 320 remainder is left as a trailing gap), where
// an hfiller would have taken it.
TEST(PureSolverLayout, vfillerDoesNotFlexCrossAxis)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idFixed = btl::makeUniqueId();
    btl::UniqueId const idFiller = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(probe(idFixed, fixed40, fixed40) | modifier::fixedWidth(80.0f));
    row.push_back(withArea(vfiller(), idFiller));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    EXPECT_FLOAT_EQ(80.0f, readProbe(instance, idFixed).size[0]);
    EXPECT_FLOAT_EQ(0.0f, readProbe(instance, idFiller).size[0]);
}

// A leaf that only sets a SizeHint -- no pure modifier -- is bridged into a pure
// band at the container: it solves to its hint's 300x300 natural, not the weak
// 100x100 default. This is the curveVisualizer / scrollbar case.
TEST(PureSolverLayout, sizeHintOnlyLeafBridgesToPureBand)
{
    avg::Vector2f const window(400.0f, 400.0f);

    btl::UniqueId const id = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> column;
    column.push_back(withArea(
            makeWidget() | modifier::setSizeHint(avg::Vector2f(300.0f, 300.0f)),
            id));

    Instance instance = realiseConverged(
            pureSolverRoot(vbox(ArraySignal<AnyWidget>(std::move(column)))),
            window);

    Geometry g = readProbe(instance, id);
    EXPECT_FLOAT_EQ(300.0f, g.size[0]);
    EXPECT_FLOAT_EQ(300.0f, g.size[1]);
}

// setSize lives in the SizeHint, so a setSize-only leaf bridges the same way: it
// solves to the requested 220x160. This is the spinner case.
TEST(PureSolverLayout, setSizeLeafBridgesToPureBand)
{
    avg::Vector2f const window(400.0f, 400.0f);

    btl::UniqueId const id = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> column;
    column.push_back(withArea(
            makeWidget() | modifier::setSize(avg::Vector2f(220.0f, 160.0f)),
            id));

    Instance instance = realiseConverged(
            pureSolverRoot(vbox(ArraySignal<AnyWidget>(std::move(column)))),
            window);

    Geometry g = readProbe(instance, id);
    EXPECT_FLOAT_EQ(220.0f, g.size[0]);
    EXPECT_FLOAT_EQ(160.0f, g.size[1]);
}

// setMinimumSize raises the SizeHint's natural to the minimum, and the bridge
// carries that up: a 40x40 leaf floored at 200x150 solves to 200x150, clamped up
// to the minimum rather than sitting at its smaller content size.
TEST(PureSolverLayout, minimumSizeLeafBridgesIntoPureBand)
{
    avg::Vector2f const window(400.0f, 400.0f);

    btl::UniqueId const id = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> column;
    column.push_back(withArea(
            makeWidget()
                | modifier::setSizeHint(avg::Vector2f(40.0f, 40.0f))
                | modifier::setMinimumSize(avg::Vector2f(200.0f, 150.0f)),
            id));

    Instance instance = realiseConverged(
            pureSolverRoot(vbox(ArraySignal<AnyWidget>(std::move(column)))),
            window);

    Geometry g = readProbe(instance, id);
    EXPECT_FLOAT_EQ(200.0f, g.size[0]);
    EXPECT_FLOAT_EQ(150.0f, g.size[1]);
}

// A container aggregates a bridged leaf's real natural, not the 100x100 default:
// a 137-wide SizeHint-only leaf beside a fixed 80 leaf in a pure hbox tiles to
// 137 + 80, so the fixed sibling sits at 137.
TEST(PureSolverLayout, bridgedLeafAggregatesInPureHbox)
{
    avg::Vector2f const window(400.0f, 100.0f);

    btl::UniqueId const idBridged = btl::makeUniqueId();
    btl::UniqueId const idFixed = btl::makeUniqueId();

    std::vector<ArraySignal<AnyWidget>> row;
    row.push_back(withArea(
            makeWidget() | modifier::setSizeHint(avg::Vector2f(137.0f, 40.0f)),
            idBridged));
    row.push_back(probe(idFixed, fixed40, fixed40) | modifier::fixedWidth(80.0f));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(ArraySignal<AnyWidget>(std::move(row)))),
            window);

    Geometry bridged = readProbe(instance, idBridged);
    Geometry fixed = readProbe(instance, idFixed);

    EXPECT_FLOAT_EQ(137.0f, bridged.size[0]);
    EXPECT_FLOAT_EQ(0.0f, bridged.position[0]);
    EXPECT_FLOAT_EQ(80.0f, fixed.size[0]);
    EXPECT_FLOAT_EQ(137.0f, fixed.position[0]);
}

// Height reflows with the resolved width. A leaf whose content height is
// area / width fills its row, so its resolved width is the window width; its
// height then follows the width solution through phase 2. In a 400-wide window
// it is 30 tall (12000 / 400); in a 200-wide one it is 60 (12000 / 200) -- taller
// where narrower. Were phase 2 blind to the resolved width, both would read the
// natural-width height and be identical.
TEST(PureSolverLayout, heightReflowsWithResolvedWidth)
{
    btl::UniqueId const id = btl::makeUniqueId();
    float const area = 12000.0f;

    auto makeTree = [&]() -> AnyWidget
    {
        std::vector<ArraySignal<AnyWidget>> row;
        row.push_back(reflowProbe(id, area, 100.0f));
        return hbox(ArraySignal<AnyWidget>(std::move(row)));
    };

    Instance wide = realiseConverged(pureSolverRoot(makeTree()),
            avg::Vector2f(400.0f, 300.0f));
    Instance narrow = realiseConverged(pureSolverRoot(makeTree()),
            avg::Vector2f(200.0f, 300.0f));

    Geometry wideG = readProbe(wide, id);
    Geometry narrowG = readProbe(narrow, id);

    EXPECT_FLOAT_EQ(400.0f, wideG.size[0]);
    EXPECT_FLOAT_EQ(30.0f, wideG.size[1]);
    EXPECT_FLOAT_EQ(200.0f, narrowG.size[0]);
    EXPECT_FLOAT_EQ(60.0f, narrowG.size[1]);
    EXPECT_GT(narrowG.size[1], wideG.size[1]);
}
