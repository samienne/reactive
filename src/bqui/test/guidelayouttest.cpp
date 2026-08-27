#include "widget/constraintlayout.h"
#include "widget/guideaccess.h"

#include <bqui/widget/guide.h>
#include <bqui/widget/resolvedguides.h>
#include <bqui/widget/widget.h>

#include <bqui/modifier/alignguide.h>

#include <bqui/buildparams.h>

#include <bq/signal/constant.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <arrange/expression.h>
#include <arrange/strength.h>

#include <avg/rendertree/uniqueid.h>

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

    // Holds a child at a fixed size and top so the only freedom left along the
    // horizontal axis is the position a guide then decides.
    void fixSizeAndTop(std::vector<arrange::Constraint>& into,
            BoxVariables const& box, float width, float height, float top)
    {
        into.push_back(
                (box.width() == arrange::Expression(width))
                | arrange::Strength::strong());
        into.push_back(
                (box.height() == arrange::Expression(height))
                | arrange::Strength::strong());
        into.push_back(
                (arrange::Expression(box.top) == arrange::Expression(top))
                | arrange::Strength::strong());
    }

    // A container always supplies every child a weak positioning default through
    // placeInSlot; a guide then pulls the edge off it at medium strength. These
    // solve-layer tests exercise guideConstraints without that container, so they
    // stand in the same weak default here. Without it the aligned edge is reached
    // by no weak constraint and is a free variable the solver may leave at its
    // undefined default, so the assertion would depend on variable-id order. The
    // default is deliberately away from the guide line, so an edge landing on the
    // line proves the guide carried it there rather than it merely staying put.
    void weakLeadingDefault(std::vector<arrange::Constraint>& into,
            arrange::Variable const& leadingEdge, float position)
    {
        into.push_back(
                (arrange::Expression(leadingEdge) == arrange::Expression(position))
                | arrange::Strength::weak());
    }
} // namespace

// alignLeft records a left-edge alignment to the named guide on the builder,
// exposed for a container to read, and carries the guide's stable identity.
TEST(guideLayout, alignLeftRecordsAlignment)
{
    XGuide g;

    auto builder = (makeWidget() | modifier::alignLeft(g))(BuildParams());

    auto const& alignments = builder.getGuideAlignments();
    ASSERT_EQ(1u, alignments.size());
    EXPECT_EQ(GuideEdge::left, alignments[0].edge);
    EXPECT_EQ(GuideAccess::id(g), alignments[0].guide);
}

// A guide is an opaque, copyable, comparable token: a copy names the same line
// as the original, and two independently minted guides differ. Alignments to
// several guides accumulate on the builder.
TEST(guideLayout, guidesAreCopyableTokensAndAccumulate)
{
    XGuide g;
    XGuide sameAsG = g;
    XGuide other;

    EXPECT_TRUE(g == sameAsG);
    EXPECT_TRUE(g != other);

    YGuide h;

    auto builder = (makeWidget()
            | modifier::alignLeft(g)
            | modifier::alignTop(h))(BuildParams());

    auto const& alignments = builder.getGuideAlignments();
    ASSERT_EQ(2u, alignments.size());
    EXPECT_EQ(GuideEdge::left, alignments[0].edge);
    EXPECT_EQ(GuideEdge::top, alignments[1].edge);
    EXPECT_EQ(GuideAccess::id(g), alignments[0].guide);
    EXPECT_EQ(GuideAccess::id(h), alignments[1].guide);
}

// Two children in one container aligning their left edges to one shared XGuide
// line up on that edge: the first is pinned at x=50, and the guide carries the
// second child's left edge to the same 50, resolved inside the one solve.
TEST(guideLayout, twoChildrenShareAnXGuideLeftEdge)
{
    BoxVariables container;
    BoxVariables a;
    BoxVariables b;
    avg::UniqueId g;

    LayoutSpec spec;
    append(spec.constraints,
            anchorConstraints(container, 0.f, 0.f, 200.f, 100.f));
    fixSizeAndTop(spec.constraints, a, 40.f, 20.f, 10.f);
    fixSizeAndTop(spec.constraints, b, 30.f, 20.f, 60.f);
    spec.constraints.push_back(
            (arrange::Expression(a.left) == arrange::Expression(50.0))
            | arrange::Strength::strong());
    weakLeadingDefault(spec.constraints, b.left, 0.f);

    std::vector<std::vector<GuideAlignment>> alignments = {
        { GuideAlignment{ g, GuideEdge::left } },
        { GuideAlignment{ g, GuideEdge::left } },
    };
    append(spec.constraints, guideConstraints({ a, b }, alignments));

    spec.variables = { a.left, a.right, b.left, b.right };

    auto solution = solveOnce(std::move(spec));

    EXPECT_DOUBLE_EQ(50.0, solution.at(a.left.id()));
    EXPECT_DOUBLE_EQ(50.0, solution.at(b.left.id()));
    EXPECT_DOUBLE_EQ(90.0, solution.at(a.right.id()));
    EXPECT_DOUBLE_EQ(80.0, solution.at(b.right.id()));
}

// A centre alignment lines the children up on their midpoints, not an edge: two
// children of different widths aligned centerX to one guide end up centred on
// the same X, so their left edges differ but their centres match.
TEST(guideLayout, centerXGuideAlignsMidpoints)
{
    BoxVariables container;
    BoxVariables a;
    BoxVariables b;
    avg::UniqueId g;

    LayoutSpec spec;
    append(spec.constraints,
            anchorConstraints(container, 0.f, 0.f, 200.f, 100.f));
    fixSizeAndTop(spec.constraints, a, 40.f, 20.f, 10.f);
    fixSizeAndTop(spec.constraints, b, 60.f, 20.f, 60.f);
    spec.constraints.push_back(
            (arrange::Expression(a.left) == arrange::Expression(50.0))
            | arrange::Strength::strong());

    std::vector<std::vector<GuideAlignment>> alignments = {
        { GuideAlignment{ g, GuideEdge::centerX } },
        { GuideAlignment{ g, GuideEdge::centerX } },
    };
    append(spec.constraints, guideConstraints({ a, b }, alignments));

    spec.variables = { a.left, a.right, b.left, b.right };

    auto solution = solveOnce(std::move(spec));

    // a spans 50..90, centre 70; b is 60 wide centred on 70, so 40..100.
    EXPECT_DOUBLE_EQ(70.0,
            (solution.at(a.left.id()) + solution.at(a.right.id())) / 2.0);
    EXPECT_DOUBLE_EQ(70.0,
            (solution.at(b.left.id()) + solution.at(b.right.id())) / 2.0);
    EXPECT_DOUBLE_EQ(40.0, solution.at(b.left.id()));
    EXPECT_DOUBLE_EQ(100.0, solution.at(b.right.id()));
}

// A YGuide positions on the vertical axis: two children aligning their top
// edges to one guide share a top.
TEST(guideLayout, twoChildrenShareAYGuideTopEdge)
{
    BoxVariables container;
    BoxVariables a;
    BoxVariables b;
    avg::UniqueId g;

    LayoutSpec spec;
    append(spec.constraints,
            anchorConstraints(container, 0.f, 0.f, 200.f, 100.f));
    // Fix each child's size and left, leaving the vertical position free.
    spec.constraints.push_back(
            (a.width() == arrange::Expression(40.0)) | arrange::Strength::strong());
    spec.constraints.push_back(
            (a.height() == arrange::Expression(20.0)) | arrange::Strength::strong());
    spec.constraints.push_back(
            (arrange::Expression(a.left) == arrange::Expression(10.0))
            | arrange::Strength::strong());
    spec.constraints.push_back(
            (b.width() == arrange::Expression(30.0)) | arrange::Strength::strong());
    spec.constraints.push_back(
            (b.height() == arrange::Expression(20.0)) | arrange::Strength::strong());
    spec.constraints.push_back(
            (arrange::Expression(b.left) == arrange::Expression(80.0))
            | arrange::Strength::strong());
    spec.constraints.push_back(
            (arrange::Expression(a.top) == arrange::Expression(25.0))
            | arrange::Strength::strong());
    weakLeadingDefault(spec.constraints, b.top, 0.f);

    std::vector<std::vector<GuideAlignment>> alignments = {
        { GuideAlignment{ g, GuideEdge::top } },
        { GuideAlignment{ g, GuideEdge::top } },
    };
    append(spec.constraints, guideConstraints({ a, b }, alignments));

    spec.variables = { a.top, a.bottom, b.top, b.bottom };

    auto solution = solveOnce(std::move(spec));

    EXPECT_DOUBLE_EQ(25.0, solution.at(a.top.id()));
    EXPECT_DOUBLE_EQ(25.0, solution.at(b.top.id()));
    EXPECT_DOUBLE_EQ(45.0, solution.at(b.bottom.id()));
}

// A guide already resolved by an ancestor firewall is pinned to that inherited
// constant rather than resolved as a free variable. The child's left edge is
// otherwise free; the resolved map fixes the guide line at 80, so the aligned
// edge lands there. Compare singleParticipantResolves, where the same lone guide
// with no inherited value settles at the child's own edge instead.
TEST(guideLayout, inheritedGuideIsPinnedToTheConstant)
{
    BoxVariables container;
    BoxVariables a;
    avg::UniqueId g;

    LayoutSpec spec;
    append(spec.constraints,
            anchorConstraints(container, 0.f, 0.f, 200.f, 100.f));
    fixSizeAndTop(spec.constraints, a, 40.f, 20.f, 10.f);
    weakLeadingDefault(spec.constraints, a.left, 0.f);

    ResolvedGuideMap resolved{ { g, 80.0f } };
    guideConstraints(spec.constraints, { a },
            { { GuideAlignment{ g, GuideEdge::left } } }, resolved);

    spec.variables = { a.left, a.right };

    auto solution = solveOnce(std::move(spec));

    // The inherited line fixes the left edge at 80; the width band keeps its 40.
    EXPECT_DOUBLE_EQ(80.0, solution.at(a.left.id()));
    EXPECT_DOUBLE_EQ(120.0, solution.at(a.right.id()));
}

// The cross-firewall protocol at the solve layer. An outer firewall resolves a
// guide by coupling two of its widgets, its solved line is read back out of the
// solution, and that constant is handed to a separate inner firewall solve where
// a widget pins to the very same line. Two independent solves — two firewalls —
// meet on one guide because its value crossed the boundary as a constant, which
// is what the resolved-guide down-channel carries.
TEST(guideLayout, resolvedGuideValueCrossesBetweenFirewallSolves)
{
    avg::UniqueId g;

    // Outer firewall: oa is pinned at x=50 and shares guide g with ob, so the
    // solve settles the shared line at 50. guideConstraints hands back the line
    // variable it minted, which travels with the spec to be read back.
    BoxVariables outerContainer;
    BoxVariables oa;
    BoxVariables ob;

    LayoutSpec outerSpec;
    append(outerSpec.constraints,
            anchorConstraints(outerContainer, 0.f, 0.f, 200.f, 100.f));
    fixSizeAndTop(outerSpec.constraints, oa, 40.f, 20.f, 10.f);
    fixSizeAndTop(outerSpec.constraints, ob, 30.f, 20.f, 60.f);
    outerSpec.constraints.push_back(
            (arrange::Expression(oa.left) == arrange::Expression(50.0))
            | arrange::Strength::strong());
    weakLeadingDefault(outerSpec.constraints, ob.left, 0.f);

    std::vector<std::vector<GuideAlignment>> outerAlignments = {
        { GuideAlignment{ g, GuideEdge::left } },
        { GuideAlignment{ g, GuideEdge::left } },
    };
    auto outerLines = guideConstraints(outerSpec.constraints, { oa, ob },
            outerAlignments, ResolvedGuideMap());

    ASSERT_EQ(1u, outerLines.count(g));
    arrange::Variable const& gLine = outerLines.at(g);
    outerSpec.variables = { oa.left, ob.left, gLine };

    auto outerSolution = solveOnce(std::move(outerSpec));
    float gValue = static_cast<float>(outerSolution.at(gLine.id()));
    EXPECT_FLOAT_EQ(50.0f, gValue);

    // Down-channel: the outer firewall exports what it resolved.
    ResolvedGuideMap resolved{ { g, gValue } };

    // Inner firewall: a fresh solve that never resolves g itself. It only knows
    // g through the inherited constant, yet its widget lands on the same line.
    BoxVariables innerContainer;
    BoxVariables ic;

    LayoutSpec innerSpec;
    append(innerSpec.constraints,
            anchorConstraints(innerContainer, 0.f, 0.f, 200.f, 100.f));
    fixSizeAndTop(innerSpec.constraints, ic, 30.f, 20.f, 40.f);
    weakLeadingDefault(innerSpec.constraints, ic.left, 0.f);
    guideConstraints(innerSpec.constraints, { ic },
            { { GuideAlignment{ g, GuideEdge::left } } }, resolved);

    innerSpec.variables = { ic.left };

    auto innerSolution = solveOnce(std::move(innerSpec));
    EXPECT_DOUBLE_EQ(50.0, innerSolution.at(ic.left.id()));
}

// A guide with a single participant is not an error: the child keeps its size
// and the guide simply settles at the child's edge, so the solve resolves and
// the box reads back cleanly.
TEST(guideLayout, singleParticipantResolves)
{
    BoxVariables container;
    BoxVariables a;
    avg::UniqueId g;

    LayoutSpec spec;
    append(spec.constraints,
            anchorConstraints(container, 0.f, 0.f, 200.f, 100.f));
    fixSizeAndTop(spec.constraints, a, 40.f, 20.f, 10.f);

    std::vector<std::vector<GuideAlignment>> alignments = {
        { GuideAlignment{ g, GuideEdge::left } },
    };
    append(spec.constraints, guideConstraints({ a }, alignments));

    spec.variables = { a.left, a.right };

    auto solution = solveOnce(std::move(spec));

    // The width band is untouched by the lone guide, so the box keeps its 40.
    EXPECT_DOUBLE_EQ(40.0,
            solution.at(a.right.id()) - solution.at(a.left.id()));
}
