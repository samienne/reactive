#include "purelayouttestutil.h"

#include <bqui/modifier/alignguide.h>
#include <bqui/modifier/constraintsize.h>
#include <bqui/modifier/margin.h>
#include <bqui/modifier/setanchor.h>
#include <bqui/modifier/setgravity.h>
#include <bqui/modifier/settheme.h>

#include <bqui/widget/filler.h>
#include <bqui/widget/guide.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/label.h>
#include <bqui/widget/vbox.h>

#include <bqui/theme.h>

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

// A fixed-size leaf publishing a baseline @p baseline below its top edge.
AnyWidget baselineProbe(btl::UniqueId id, float width, float height,
        float baseline)
{
    return withArea(makeWidget()
            | modifier::defaultSize(avg::Vector2f(width, height))
            | modifier::setAnchor(baselineAnchor,
                constant(Anchor{ 0.0f, baseline })),
            id);
}

// A fixed-size leaf with no baseline.
AnyWidget plainProbe(btl::UniqueId id, float width, float height)
{
    return probe(id, Band{ width, width, width }, Band{ height, height, height });
}

ArraySignal<AnyWidget> list(std::vector<AnyWidget> widgets)
{
    std::vector<ArraySignal<AnyWidget>> items;
    for (auto& w : widgets)
        items.push_back(std::move(w));
    return ArraySignal<AnyWidget>(std::move(items));
}

// Puts @p row at the top of a column whose filler takes the rest, so the row
// settles at its own natural height.
AnyWidget atTop(btl::UniqueId rowId, AnyWidget row)
{
    return vbox(list({ withArea(std::move(row), rowId), vfiller() }));
}

// The top edge of a probe, top down in window space.
float top(Geometry const& g)
{
    return window[1] - g.position[1] - g.size[1];
}

float bottom(Geometry const& g)
{
    return window[1] - g.position[1];
}

// A row holding @p child at the region's left edge, free to move right.
AnyWidget leftRow(AnyWidget child)
{
    return hbox(list({ std::move(child) }))
        | modifier::setGravity(constant(avg::Vector2f(0.0f, 0.5f)));
}

TEST(AnchorLayout, rowAlignsBaselinesAndAggregatesHeight)
{
    btl::UniqueId const idRow = btl::makeUniqueId();
    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();
    btl::UniqueId const idC = btl::makeUniqueId();

    Instance instance = realiseConverged(atTop(idRow, baselineHbox(list({
            baselineProbe(idA, 30.0f, 40.0f, 30.0f),
            baselineProbe(idB, 30.0f, 60.0f, 50.0f),
            baselineProbe(idC, 30.0f, 20.0f, 5.0f) }))),
            window);

    Geometry row = readProbe(instance, idRow);
    Geometry a = readProbe(instance, idA);
    Geometry b = readProbe(instance, idB);
    Geometry c = readProbe(instance, idC);

    // Deepest ascent 50 (b) plus deepest descent 15 (c).
    EXPECT_NEAR(65.0f, row.size[1], 0.01f);
    EXPECT_NEAR(0.0f, top(row), 0.01f);

    EXPECT_NEAR(40.0f, a.size[1], 0.01f);
    EXPECT_NEAR(60.0f, b.size[1], 0.01f);
    EXPECT_NEAR(20.0f, c.size[1], 0.01f);

    EXPECT_NEAR(50.0f, top(a) + 30.0f, 0.01f);
    EXPECT_NEAR(50.0f, top(b) + 50.0f, 0.01f);
    EXPECT_NEAR(50.0f, top(c) + 5.0f, 0.01f);
}

TEST(AnchorLayout, labelsOfDifferentSizesShareABaseline)
{
    btl::UniqueId const idRow = btl::makeUniqueId();
    btl::UniqueId const idSmall = btl::makeUniqueId();
    btl::UniqueId const idLarge = btl::makeUniqueId();

    Theme small;
    Theme large;
    large.setTextHeight(2.5f * small.getTextHeight());

    Instance instance = realiseConverged(atTop(idRow, baselineHbox(list({
            withArea(label(std::string("Small"))
                | modifier::setTheme(small), idSmall),
            withArea(label(std::string("Large"))
                | modifier::setTheme(large), idLarge) }))),
            window);

    Geometry row = readProbe(instance, idRow);
    Geometry s = readProbe(instance, idSmall);
    Geometry l = readProbe(instance, idLarge);

    ASSERT_GT(l.size[1], s.size[1]);

    // A label centres its text, so its baseline sits half its height plus
    // half the text height less the (negative) descender below its top; the
    // margin inset cancels at the centre.
    auto baseline = [](Geometry const& g, Theme const& theme)
    {
        float h = theme.getTextHeight();
        return top(g) + 0.5f * g.size[1] + 0.5f * h
            - theme.getFont().getDescender(h);
    };

    EXPECT_NEAR(baseline(s, small), baseline(l, large), 0.01f);

    // Each label keeps its own height, and the row is tall enough for both.
    EXPECT_GE(row.size[1] + 0.01f, l.size[1]);
    EXPECT_GE(bottom(row) + 0.01f, bottom(s));
    EXPECT_GE(bottom(row) + 0.01f, bottom(l));
    EXPECT_LE(top(row) - 0.01f, top(s));
}

TEST(AnchorLayout, marginOffsetsTheAnchor)
{
    btl::UniqueId const idRow = btl::makeUniqueId();
    btl::UniqueId const idInner = btl::makeUniqueId();
    btl::UniqueId const idPlain = btl::makeUniqueId();

    Instance instance = realiseConverged(atTop(idRow, baselineHbox(list({
            baselineProbe(idInner, 30.0f, 40.0f, 30.0f)
                | modifier::margin(constant(10.0f)),
            baselineProbe(idPlain, 30.0f, 40.0f, 30.0f) }))),
            window);

    Geometry row = readProbe(instance, idRow);
    Geometry inner = readProbe(instance, idInner);
    Geometry plain = readProbe(instance, idPlain);

    // The margined child's baseline is 40 below its outer top and its descent
    // 20, so the row is 60 tall and the plain child sits 10 lower.
    EXPECT_NEAR(60.0f, row.size[1], 0.01f);
    EXPECT_NEAR(40.0f, inner.size[1], 0.01f);
    EXPECT_NEAR(10.0f, top(inner), 0.01f);
    EXPECT_NEAR(top(inner) + 30.0f, top(plain) + 30.0f, 0.01f);
}

TEST(AnchorLayout, nestedRowsPublishTheirBaseline)
{
    btl::UniqueId const idRow = btl::makeUniqueId();
    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();
    btl::UniqueId const idC = btl::makeUniqueId();

    Instance instance = realiseConverged(atTop(idRow, baselineHbox(list({
            baselineProbe(idA, 30.0f, 40.0f, 30.0f),
            baselineHbox(list({
                baselineProbe(idB, 30.0f, 20.0f, 5.0f),
                baselineProbe(idC, 30.0f, 60.0f, 50.0f) })) }))),
            window);

    Geometry row = readProbe(instance, idRow);
    Geometry a = readProbe(instance, idA);
    Geometry b = readProbe(instance, idB);
    Geometry c = readProbe(instance, idC);

    // The inner row is 50 + 15 tall with its baseline 50 down; the outer row
    // adds nothing above or below it.
    EXPECT_NEAR(65.0f, row.size[1], 0.01f);
    EXPECT_NEAR(50.0f, top(a) + 30.0f, 0.01f);
    EXPECT_NEAR(50.0f, top(b) + 5.0f, 0.01f);
    EXPECT_NEAR(50.0f, top(c) + 50.0f, 0.01f);
}

TEST(AnchorLayout, childWithoutBaselineAlignsItsBottom)
{
    btl::UniqueId const idRow = btl::makeUniqueId();
    btl::UniqueId const idText = btl::makeUniqueId();
    btl::UniqueId const idBox = btl::makeUniqueId();
    btl::UniqueId const idFill = btl::makeUniqueId();

    Instance instance = realiseConverged(atTop(idRow, baselineHbox(list({
            baselineProbe(idText, 30.0f, 40.0f, 30.0f),
            plainProbe(idBox, 30.0f, 50.0f),
            withArea(makeWidget() | modifier::growHeight(), idFill) }))),
            window);

    Geometry row = readProbe(instance, idRow);
    Geometry text = readProbe(instance, idText);
    Geometry box = readProbe(instance, idBox);
    Geometry fill = readProbe(instance, idFill);

    // The box's bottom is its baseline: ascent 50, descent 10 from the text.
    EXPECT_NEAR(60.0f, row.size[1], 0.01f);
    EXPECT_NEAR(50.0f, bottom(box), 0.01f);
    EXPECT_NEAR(50.0f, top(text) + 30.0f, 0.01f);

    // A child with no height of its own fills the row as in an hbox.
    EXPECT_NEAR(top(row), top(fill), 0.01f);
    EXPECT_NEAR(row.size[1], fill.size[1], 0.01f);
}

// A user-defined key published with setAnchor reaches alignAnchor: the two
// anchor points meet on the guide, at the furthest of them.
TEST(AnchorLayout, customAnchorKeyBindsToAGuide)
{
    XAnchorKey const notch;
    XGuide line;
    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();

    auto notched = [&](btl::UniqueId id, float at)
    {
        return probe(id, Band{ 60.0f, 60.0f, 60.0f },
                    Band{ 20.0f, 20.0f, 20.0f })
            | modifier::setAnchor(notch, constant(Anchor{ 0.0f, at }))
            | modifier::alignAnchor(line, notch);
    };

    Instance instance = realiseConverged(vbox(list({
            leftRow(notched(idA, 10.0f)),
            leftRow(notched(idB, 40.0f)),
            vfiller() })),
            window);

    Geometry a = readProbe(instance, idA);
    Geometry b = readProbe(instance, idB);

    EXPECT_NEAR(40.0f, a.position[0] + 10.0f, 0.01f);
    EXPECT_NEAR(40.0f, b.position[0] + 40.0f, 0.01f);
}

// A key is its own identity: a key that was never published binds nothing,
// even on a widget that publishes another key.
TEST(AnchorLayout, distinctKeysDoNotAlias)
{
    XAnchorKey const published;
    XAnchorKey const other;
    XGuide line;
    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();

    EXPECT_NE(published, other);

    Instance instance = realiseConverged(vbox(list({
            leftRow(probe(idA, Band{ 60.0f, 60.0f, 60.0f },
                        Band{ 20.0f, 20.0f, 20.0f })
                | modifier::setAnchor(published,
                    constant(Anchor{ 0.0f, 10.0f }))
                | modifier::alignAnchor(line, other)),
            leftRow(probe(idB, Band{ 60.0f, 60.0f, 60.0f },
                        Band{ 20.0f, 20.0f, 20.0f })
                | modifier::setAnchor(published,
                    constant(Anchor{ 0.0f, 40.0f }))
                | modifier::alignAnchor(line, published)),
            vfiller() })),
            window);

    EXPECT_NEAR(0.0f, readProbe(instance, idA).position[0], 0.01f);
    EXPECT_NEAR(0.0f, readProbe(instance, idB).position[0], 0.01f);
}

TEST(AnchorLayout, tallerRowCentresTheAlignedBlock)
{
    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();

    // The row is the region's root, so it takes the whole window height.
    Instance instance = realiseConverged(baselineHbox(list({
            baselineProbe(idA, 30.0f, 40.0f, 30.0f),
            baselineProbe(idB, 30.0f, 60.0f, 50.0f) })),
            window);

    Geometry a = readProbe(instance, idA);
    Geometry b = readProbe(instance, idB);

    // The 60-tall block is centred: its top at 120, its baseline at 170.
    EXPECT_NEAR(120.0f, top(b), 0.01f);
    EXPECT_NEAR(170.0f, top(a) + 30.0f, 0.01f);
}

} // namespace
