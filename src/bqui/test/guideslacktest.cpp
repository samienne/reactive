#include "purelayouttestutil.h"

#include <bqui/modifier/alignguide.h>
#include <bqui/modifier/constraintsize.h>
#include <bqui/modifier/setanchor.h>

#include <bqui/widget/filler.h>
#include <bqui/widget/guide.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/stack.h>
#include <bqui/widget/uniformgrid.h>
#include <bqui/widget/vbox.h>

#include <bq/signal/arraysignal.h>

#include <string>
#include <utility>
#include <vector>

using namespace bqui;
using namespace bqui::widget;
using namespace bq::signal;
using namespace bqui::test;

namespace
{

avg::Vector2f const window(400.0f, 300.0f);

ArraySignal<AnyWidget> list(std::vector<AnyWidget> widgets)
{
    std::vector<ArraySignal<AnyWidget>> items;
    for (auto& w : widgets)
        items.push_back(std::move(w));
    return ArraySignal<AnyWidget>(std::move(items));
}

AnyWidget leaf(btl::UniqueId id, float width, float height = 20.0f)
{
    return probe(id, Band{ width, width, width },
            Band{ height, height, height });
}

AnyWidget fillLeaf(btl::UniqueId id, float width)
{
    return probe(id, Band{ width, width, 10000.0f, 1.0f },
            Band{ 20.0f, 20.0f, 20.0f });
}

AnyWidget baselineLeaf(btl::UniqueId id, float height, float baseline)
{
    return withArea(makeWidget()
            | modifier::defaultSize(avg::Vector2f(30.0f, height))
            | modifier::setAnchor(baselineAnchor,
                constant(Anchor{ 0.0f, baseline })),
            id);
}

float left(Geometry const& g)
{
    return g.position[0];
}

float right(Geometry const& g)
{
    return g.position[0] + g.size[0];
}

float top(Geometry const& g)
{
    return window[1] - g.position[1] - g.size[1];
}

float width(Geometry const& g)
{
    return g.size[0];
}

struct Solved
{
    Instance instance;
    std::string log;

    Geometry operator[](btl::UniqueId id) const
    {
        return readProbe(instance, id);
    }

    bool guideUnmet() const
    {
        return log.find("unmet: guide") != std::string::npos;
    }
};

Solved solve(AnyWidget widget, avg::Vector2f size = window)
{
    testing::internal::CaptureStderr();
    Instance instance = realiseConverged(std::move(widget), size);
    return Solved{ std::move(instance),
        testing::internal::GetCapturedStderr() };
}

struct Row
{
    btl::UniqueId label = btl::makeUniqueId();
    btl::UniqueId field = btl::makeUniqueId();
};

AnyWidget formRow(Row const& ids, float labelWidth, XGuide guide,
        float fieldWidth = 100.0f)
{
    return hbox(list({
            leaf(ids.label, labelWidth) | modifier::alignRight(guide),
            leaf(ids.field, fieldWidth) }));
}

} // namespace

TEST(GuideSlack, classicFormShiftsTheShorterRowOffCentre)
{
    XGuide g;
    Row r1, r2;
    Solved s = solve(vbox(list({ formRow(r1, 50.0f, g),
                formRow(r2, 80.0f, g) })));

    EXPECT_NEAR(190.0f, left(s[r1.field]), 0.01f);
    EXPECT_NEAR(190.0f, left(s[r2.field]), 0.01f);
    EXPECT_NEAR(50.0f, width(s[r1.label]), 0.01f);
    EXPECT_NEAR(140.0f, left(s[r1.label]), 0.01f);
    EXPECT_NEAR(110.0f, left(s[r2.label]), 0.01f);
    EXPECT_FALSE(s.guideUnmet()) << s.log;
}

TEST(GuideSlack, nestedRowsShiftTheirWrapperLikeAFlatRow)
{
    XGuide g;
    Row r1, r2;
    Solved s = solve(vbox(list({
            hbox(list({ formRow(r1, 50.0f, g) })),
            vbox(list({ formRow(r2, 80.0f, g) })) })));

    EXPECT_NEAR(190.0f, left(s[r1.field]), 0.01f);
    EXPECT_NEAR(190.0f, left(s[r2.field]), 0.01f);
    EXPECT_NEAR(50.0f, width(s[r1.label]), 0.01f);
    EXPECT_NEAR(140.0f, left(s[r1.label]), 0.01f);
    EXPECT_FALSE(s.guideUnmet()) << s.log;
}

TEST(GuideSlack, formWithNoLeftoverWidthOverflowsTheRegionSilently)
{
    XGuide g;
    Row r1, r2;
    Solved s = solve(vbox(list({ formRow(r1, 50.0f, g, 200.0f),
                formRow(r2, 80.0f, g) })), avg::Vector2f(250.0f, 300.0f));

    EXPECT_NEAR(115.0f, left(s[r1.field]), 0.01f);
    EXPECT_NEAR(115.0f, left(s[r2.field]), 0.01f);
    EXPECT_NEAR(50.0f, width(s[r1.label]), 0.01f);
    EXPECT_NEAR(315.0f, right(s[r1.field]), 0.01f);
    EXPECT_TRUE(s.log.empty()) << s.log;
}

TEST(GuideSlack, formWithNoLeftoverWidthOverlapsItsNextSibling)
{
    XGuide g;
    Row r1, r2;
    btl::UniqueId const side = btl::makeUniqueId();
    Solved s = solve(hbox(list({
            vbox(list({ formRow(r1, 50.0f, g, 200.0f),
                    formRow(r2, 80.0f, g) })),
            leaf(side, 50.0f) })));

    EXPECT_NEAR(left(s[r2.field]), left(s[r1.field]), 0.01f);
    EXPECT_NEAR(250.0f, left(s[side]), 0.01f);
    EXPECT_NEAR(315.0f, right(s[r1.field]), 0.01f);
    EXPECT_TRUE(s.log.empty()) << s.log;
}

TEST(GuideSlack, alignedFieldEdgesShiftTheRowLikeAlignedLabelEdges)
{
    XGuide g;
    Row r1, r2;
    auto row = [g](Row const& ids, float labelWidth)
    {
        return hbox(list({ leaf(ids.label, labelWidth),
                leaf(ids.field, 100.0f) | modifier::alignLeft(g) }));
    };
    Solved s = solve(vbox(list({ row(r1, 50.0f), row(r2, 80.0f) })));

    EXPECT_NEAR(190.0f, left(s[r1.field]), 0.01f);
    EXPECT_NEAR(190.0f, left(s[r2.field]), 0.01f);
    EXPECT_NEAR(50.0f, width(s[r1.label]), 0.01f);
    EXPECT_NEAR(140.0f, left(s[r1.label]), 0.01f);
    EXPECT_FALSE(s.guideUnmet()) << s.log;
}

TEST(GuideSlack, yGuideAcrossColumnsShiftsTheShorterColumnDown)
{
    YGuide g;
    btl::UniqueId const a1 = btl::makeUniqueId();
    btl::UniqueId const a2 = btl::makeUniqueId();
    btl::UniqueId const b1 = btl::makeUniqueId();
    btl::UniqueId const b2 = btl::makeUniqueId();
    Solved s = solve(hbox(list({
            vbox(list({ leaf(a1, 50.0f, 20.0f),
                    leaf(a2, 50.0f, 20.0f) | modifier::alignTop(g) })),
            vbox(list({ leaf(b1, 50.0f, 40.0f),
                    leaf(b2, 50.0f, 20.0f) | modifier::alignTop(g) })) })));

    EXPECT_NEAR(160.0f, top(s[a2]), 0.01f);
    EXPECT_NEAR(160.0f, top(s[b2]), 0.01f);
    EXPECT_NEAR(20.0f, s[a1].size[1], 0.01f);
    EXPECT_NEAR(140.0f, top(s[a1]), 0.01f);
    EXPECT_NEAR(120.0f, top(s[b1]), 0.01f);
    EXPECT_FALSE(s.guideUnmet()) << s.log;
}

TEST(GuideSlack, yGuideAcrossFilledColumnsShiftsAWholeColumnPastTheBottom)
{
    YGuide g;
    btl::UniqueId const a1 = btl::makeUniqueId();
    btl::UniqueId const a2 = btl::makeUniqueId();
    btl::UniqueId const b1 = btl::makeUniqueId();
    btl::UniqueId const b2 = btl::makeUniqueId();
    Solved s = solve(hbox(list({
            vbox(list({ leaf(a1, 50.0f, 20.0f),
                    leaf(a2, 50.0f, 20.0f) | modifier::alignTop(g),
                    vfiller() })),
            vbox(list({ leaf(b1, 50.0f, 40.0f),
                    leaf(b2, 50.0f, 20.0f) | modifier::alignTop(g),
                    vfiller() })) })));

    EXPECT_NEAR(40.0f, top(s[a2]), 0.01f);
    EXPECT_NEAR(40.0f, top(s[b2]), 0.01f);
    EXPECT_NEAR(20.0f, s[a1].size[1], 0.01f);
    EXPECT_NEAR(20.0f, top(s[a1]), 0.01f);
    EXPECT_NEAR(0.0f, top(s[b1]), 0.01f);
    EXPECT_FALSE(s.guideUnmet()) << s.log;
}

TEST(GuideSlack, sideBySideGridCardsShareOneAbsoluteLineAndLeaveTheirCell)
{
    XGuide g;
    Row r1, r2;
    Solved s = solve(uniformGrid(2, 1)
            .cell(0, 0, 1, 1, formRow(r1, 50.0f, g))
            .cell(1, 0, 1, 1, formRow(r2, 80.0f, g)));

    EXPECT_NEAR(290.0f, left(s[r1.field]), 0.01f);
    EXPECT_NEAR(290.0f, left(s[r2.field]), 0.01f);
    EXPECT_NEAR(240.0f, left(s[r1.label]), 0.01f);
    EXPECT_FALSE(s.guideUnmet()) << s.log;
}

TEST(GuideSlack, stackedGridCardsShiftWithinTheirCells)
{
    XGuide g;
    Row r1, r2;
    Solved s = solve(uniformGrid(1, 2)
            .cell(0, 1, 1, 1, formRow(r1, 50.0f, g))
            .cell(0, 0, 1, 1, formRow(r2, 80.0f, g)));

    EXPECT_NEAR(190.0f, left(s[r1.field]), 0.01f);
    EXPECT_NEAR(190.0f, left(s[r2.field]), 0.01f);
    EXPECT_NEAR(140.0f, left(s[r1.label]), 0.01f);
    EXPECT_NEAR(50.0f, width(s[r1.label]), 0.01f);
    EXPECT_FALSE(s.guideUnmet()) << s.log;
}

TEST(GuideSlack, fillingFieldRowShiftsAndOverflowsRatherThanShrinkingTheField)
{
    XGuide g;
    Row r1, r2;
    auto row = [g](Row const& ids, float labelWidth)
    {
        return hbox(list({
                leaf(ids.label, labelWidth) | modifier::alignRight(g),
                fillLeaf(ids.field, 100.0f) }));
    };
    Solved s = solve(vbox(list({ row(r1, 50.0f), row(r2, 80.0f) })));

    EXPECT_NEAR(80.0f, left(s[r1.field]), 0.01f);
    EXPECT_NEAR(80.0f, left(s[r2.field]), 0.01f);
    EXPECT_NEAR(50.0f, width(s[r1.label]), 0.01f);
    EXPECT_NEAR(30.0f, left(s[r1.label]), 0.01f);
    EXPECT_NEAR(430.0f, right(s[r1.field]), 0.01f);
    EXPECT_NEAR(400.0f, right(s[r2.field]), 0.01f);
    EXPECT_TRUE(s.log.empty()) << s.log;
}

TEST(GuideSlack, fixedSizeRowsStillAlignByMovingTheRow)
{
    XGuide g;
    Row r1, r2;
    auto row = [g](Row const& ids, float labelWidth)
    {
        return hbox(list({
                leaf(ids.label, labelWidth) | modifier::fixedWidth(labelWidth)
                    | modifier::alignRight(g),
                leaf(ids.field, 100.0f) | modifier::fixedWidth(100.0f) }));
    };
    Solved s = solve(vbox(list({ row(r1, 50.0f), row(r2, 80.0f) })));

    EXPECT_NEAR(190.0f, left(s[r1.field]), 0.01f);
    EXPECT_NEAR(190.0f, left(s[r2.field]), 0.01f);
    EXPECT_NEAR(50.0f, width(s[r1.label]), 0.01f);
    EXPECT_FALSE(s.guideUnmet()) << s.log;
}

TEST(GuideSlack, stackChildrenShiftWithinTheirSlots)
{
    XGuide g;
    btl::UniqueId const a = btl::makeUniqueId();
    btl::UniqueId const b = btl::makeUniqueId();
    Solved s = solve(vbox(list({
            stack({ leaf(a, 50.0f) | modifier::alignRight(g) }),
            stack({ leaf(b, 80.0f) | modifier::alignRight(g) }) })));

    EXPECT_NEAR(240.0f, right(s[a]), 0.01f);
    EXPECT_NEAR(240.0f, right(s[b]), 0.01f);
    EXPECT_NEAR(50.0f, width(s[a]), 0.01f);
    EXPECT_FALSE(s.guideUnmet()) << s.log;
}

TEST(GuideSlack, twoPointsInOneRowSqueezeTheWidgetBetweenThemToZero)
{
    XGuide g;
    Row r1, r2;
    btl::UniqueId const extra = btl::makeUniqueId();
    Solved s = solve(vbox(list({
            hbox(list({ leaf(r1.label, 50.0f) | modifier::alignRight(g),
                    leaf(r1.field, 100.0f),
                    leaf(extra, 30.0f) | modifier::alignLeft(g) })),
            formRow(r2, 80.0f, g) })));

    EXPECT_NEAR(0.0f, width(s[r1.field]), 0.01f);
    EXPECT_NEAR(260.0f, right(s[r1.label]), 0.01f);
    EXPECT_NEAR(260.0f, left(s[extra]), 0.01f);
    EXPECT_NEAR(260.0f, right(s[r2.label]), 0.01f);
    EXPECT_FALSE(s.guideUnmet()) << s.log;
}

TEST(GuideSlack, topsInOneBaselineRowStayOnTheLineAndTheGuideIsLogged)
{
    YGuide g;
    btl::UniqueId const a = btl::makeUniqueId();
    btl::UniqueId const b = btl::makeUniqueId();
    Solved s = solve(vbox(list({ baselineHbox(list({
                    baselineLeaf(a, 20.0f, 15.0f) | modifier::alignTop(g),
                    baselineLeaf(b, 40.0f, 30.0f) | modifier::alignTop(g)
                    })) })));

    EXPECT_NEAR(15.0f, top(s[a]), 0.01f);
    EXPECT_NEAR(0.0f, top(s[b]), 0.01f);
    EXPECT_TRUE(s.guideUnmet()) << s.log;
}
