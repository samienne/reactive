#include "purelayouttestutil.h"

#include "widget/constraintbox.h"

#include <bqui/modifier/constraintsize.h>

#include <bqui/widget/bin.h>
#include <bqui/widget/filler.h>
#include <bqui/widget/scrollview.h>
#include <bqui/widget/vbox.h>
#include <bqui/widget/widget.h>

#include <bqui/buildparams.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/constant.h>
#include <bq/signal/signalcontext.h>

#include <avg/vector.h>

#include <btl/uniqueid.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

// A size boundary (makeWidgetWithSize, bin, scrollView) publishes a band set
// from outside -- by size words on the widget or its own defaults -- and solves
// its content as a separate region at the size it is assigned.

using namespace bqui;
using namespace bqui::widget;
using namespace bq::signal;
using namespace bqui::test;

namespace
{

Constraints widthBandOf(AnyWidget const& widget)
{
    auto builder = widget.clone()(BuildParams());
    return makeSignalContext(builder.getPureLayout().getWidth())
        .evaluate<0>().get<0>();
}

Constraints heightBandOf(AnyWidget const& widget)
{
    auto builder = widget.clone()(BuildParams());
    return makeSignalContext(builder.getPureLayout().getHeightForWidth(
                constant(LayoutSolution()))).evaluate<0>().get<0>();
}

void expectNoBand(Constraints const& c)
{
    EXPECT_FALSE(c.min.has_value());
    EXPECT_FALSE(c.max.has_value());
    EXPECT_FALSE(c.natural.has_value());
    EXPECT_FALSE(c.flex.has_value());
}

// A column of two leaves that only grow, so their sizes come from whatever the
// column is solved at.
AnyWidget growingColumn(btl::UniqueId idA, btl::UniqueId idB)
{
    Band const grows = { 0.0f, 0.0f, 10000.0f, 1.0f };

    std::vector<ArraySignal<AnyWidget>> column;
    column.push_back(probe(idA, grows, grows));
    column.push_back(probe(idB, grows, grows));
    return vbox(ArraySignal<AnyWidget>(std::move(column)));
}

RegionBand regionBandOf(AnyWidget const& widget, avg::Vector2f size)
{
    PureRegion region = buildPureRegion(widget, constant(size), BuildParams());
    return makeSignalContext(std::move(region.band)).evaluate<0>().get<0>();
}

} // namespace

TEST(SizeBoundary, containerInsideMakeWidgetWithSizeLaysOutAtAssignedSize)
{
    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();

    AnyWidget sized = makeWidgetWithSize(
            [idA, idB](AnySignal<avg::Vector2f>)
            {
                return growingColumn(idA, idB);
            })
        | modifier::fixedSize(constant(avg::Vector2f(100.0f, 120.0f)));

    std::vector<ArraySignal<AnyWidget>> outer;
    outer.push_back(std::move(sized));
    outer.push_back(filler());

    Instance instance = realiseConverged(
            vbox(ArraySignal<AnyWidget>(std::move(outer))),
            avg::Vector2f(300.0f, 400.0f));

    Geometry a = readProbe(instance, idA);
    Geometry b = readProbe(instance, idB);

    for (Geometry const& g : { a, b })
    {
        EXPECT_FLOAT_EQ(100.0f, g.size[0]);
        EXPECT_FLOAT_EQ(60.0f, g.size[1]);
    }
    EXPECT_FLOAT_EQ(60.0f, std::abs(a.position[1] - b.position[1]));
}

TEST(SizeBoundary, makeWidgetWithSizeBandIgnoresContent)
{
    auto withContent = []()
    {
        return makeWidgetWithSize([](AnySignal<avg::Vector2f>)
                {
                    return probe(btl::makeUniqueId(), fixed100, fixed40);
                });
    };

    expectNoBand(widthBandOf(withContent()));
    expectNoBand(heightBandOf(withContent()));

    AnyWidget sized = withContent()
        | modifier::fixedSize(constant(avg::Vector2f(30.0f, 20.0f)));

    Constraints width = widthBandOf(sized);
    Constraints height = heightBandOf(sized);
    ASSERT_TRUE(width.natural.has_value());
    ASSERT_TRUE(height.natural.has_value());
    EXPECT_FLOAT_EQ(30.0f, width.natural->value);
    EXPECT_FLOAT_EQ(20.0f, height.natural->value);
}

TEST(SizeBoundary, binBandIgnoresContent)
{
    auto content = probe(btl::makeUniqueId(), fixed100, fixed40);
    AnyWidget view = bin(content, constant(avg::Vector2f(100.0f, 40.0f)));

    expectNoBand(widthBandOf(view));
    expectNoBand(heightBandOf(view));
}

TEST(SizeBoundary, scrollViewBandIgnoresContent)
{
    Band const huge = { 5000.0f, 5000.0f, 5000.0f };
    Band const tiny = { 5.0f, 5.0f, 5.0f };

    for (Band const& band : { huge, tiny })
    {
        AnyWidget view = scrollView(probe(btl::makeUniqueId(), band, band));
        AnyWidget empty = scrollView(makeWidget());

        Constraints width = widthBandOf(view);
        Constraints emptyWidth = widthBandOf(empty);
        ASSERT_TRUE(width.natural.has_value());
        ASSERT_TRUE(emptyWidth.natural.has_value());
        EXPECT_FLOAT_EQ(emptyWidth.natural->value, width.natural->value);

        Constraints height = heightBandOf(view);
        Constraints emptyHeight = heightBandOf(empty);
        ASSERT_TRUE(height.natural.has_value());
        ASSERT_TRUE(emptyHeight.natural.has_value());
        EXPECT_FLOAT_EQ(emptyHeight.natural->value, height.natural->value);
    }
}

// Content whose band states no natural is sized to the viewport, held within
// the band's bounds: a leaf that only grows fills the viewport exactly, while a
// leaf with a min taller than the viewport takes that min and scrolls.
TEST(SizeBoundary, scrollContentWithoutNaturalTakesViewport)
{
    avg::Vector2f const window(500.0f, 900.0f);
    float const bar = 25.0f;

    btl::UniqueId const idFill = btl::makeUniqueId();
    AnyWidget fills = withArea(makeWidget()
            | modifier::growWidth()
            | modifier::growHeight(), idFill);

    Instance filled = realiseConverged(scrollView(std::move(fills)), window);
    Geometry fill = readProbe(filled, idFill);
    EXPECT_FLOAT_EQ(window[0] - bar, fill.size[0]);
    EXPECT_FLOAT_EQ(window[1] - bar, fill.size[1]);

    btl::UniqueId const idTall = btl::makeUniqueId();
    AnyWidget tall = withArea(makeWidget()
            | modifier::minHeight(2000.0f)
            | modifier::growWidth()
            | modifier::growHeight(), idTall);

    Instance scrolled = realiseConverged(scrollView(std::move(tall)), window);
    Geometry t = readProbe(scrolled, idTall);
    EXPECT_FLOAT_EQ(window[0] - bar, t.size[0]);
    EXPECT_FLOAT_EQ(2000.0f, t.size[1]);
}

TEST(SizeBoundary, regionPublishesContentBand)
{
    Band const width = { 50.0f, 80.0f, 200.0f };
    Band const height = { 20.0f, 30.0f, 60.0f };

    RegionBand band = regionBandOf(probe(btl::makeUniqueId(), width, height),
            avg::Vector2f(300.0f, 400.0f));

    ASSERT_TRUE(band.width.min.has_value());
    ASSERT_TRUE(band.width.max.has_value());
    ASSERT_TRUE(band.width.natural.has_value());
    EXPECT_FLOAT_EQ(50.0f, *band.width.min);
    EXPECT_FLOAT_EQ(200.0f, *band.width.max);
    EXPECT_FLOAT_EQ(80.0f, band.width.natural->value);
    EXPECT_FALSE(band.width.flex.has_value());

    ASSERT_TRUE(band.height.min.has_value());
    ASSERT_TRUE(band.height.max.has_value());
    ASSERT_TRUE(band.height.natural.has_value());
    EXPECT_FLOAT_EQ(20.0f, *band.height.min);
    EXPECT_FLOAT_EQ(60.0f, *band.height.max);
    EXPECT_FLOAT_EQ(30.0f, band.height.natural->value);
}

TEST(SizeBoundary, regionBandAggregatesColumn)
{
    std::vector<ArraySignal<AnyWidget>> column;
    column.push_back(probe(btl::makeUniqueId(), fixed40, fixed40));
    column.push_back(probe(btl::makeUniqueId(), fixed100, fixed40));

    RegionBand band = regionBandOf(
            vbox(ArraySignal<AnyWidget>(std::move(column))),
            avg::Vector2f(300.0f, 400.0f));

    ASSERT_TRUE(band.width.natural.has_value());
    ASSERT_TRUE(band.height.natural.has_value());
    EXPECT_FLOAT_EQ(100.0f, band.width.natural->value);
    EXPECT_FLOAT_EQ(80.0f, band.height.natural->value);
}

// The height band is read at the width the region is solved at: a leaf whose
// natural height is area / width reports 6000 / 300 in a 300-wide region.
TEST(SizeBoundary, regionHeightBandIsAtSolvedWidth)
{
    RegionBand band = regionBandOf(
            reflowProbe(btl::makeUniqueId(), 6000.0f, 100.0f),
            avg::Vector2f(300.0f, 400.0f));

    ASSERT_TRUE(band.height.natural.has_value());
    EXPECT_FLOAT_EQ(20.0f, band.height.natural->value);
}
