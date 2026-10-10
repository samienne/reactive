#include <bqui/modifier/sizevocabulary.h>
#include <bqui/modifier/setsizehint.h>

#include <bqui/widget/widget.h>

#include <bqui/buildparams.h>
#include <bqui/simplesizehint.h>
#include <bqui/sizehint.h>

#include <bq/signal/signalcontext.h>

#include <avg/vector.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>

using namespace bqui;
using namespace bqui::widget;

namespace
{
    // A base widget whose width and height bands differ, so a modifier that
    // touches the wrong axis shows up immediately.
    AnyWidget base()
    {
        return makeWidget()
            | modifier::setSizeHint(simpleSizeHint(
                        Band{ 10.0f, 50.0f, 100.0f },
                        Band{ 20.0f, 60.0f, 120.0f }));
    }

    template <typename Widget>
    SizeHint buildHint(Widget widget)
    {
        auto builder = std::move(widget)(BuildParams());
        auto context = bq::signal::makeSignalContext(builder.getSizeHint());

        return context.template evaluate<0>().template get<0>();
    }

    void expectBand(std::string const& what, AxisHint hint,
            float min, float natural, float max)
    {
        EXPECT_FLOAT_EQ(min, hint.extent.min) << what << " min";
        EXPECT_FLOAT_EQ(natural, hint.extent.natural) << what << " natural";
        EXPECT_FLOAT_EQ(max, hint.extent.max) << what << " max";
    }
} // namespace

TEST(sizeVocabulary, widthAtLeastRaisesTheMinimum)
{
    SizeHint hint = buildHint(base() | modifier::widthAtLeast(30.0f));

    expectBand("width", hint.getWidth(), 30.0f, 50.0f, 100.0f);
    expectBand("height", hint.getHeightForWidth(100.0f), 20.0f, 60.0f, 120.0f);
}

TEST(sizeVocabulary, widthAtLeastCarriesNaturalAndMaxUp)
{
    SizeHint hint = buildHint(base() | modifier::widthAtLeast(150.0f));

    expectBand("width", hint.getWidth(), 150.0f, 150.0f, 150.0f);
}

TEST(sizeVocabulary, widthAtMostLowersTheMaximum)
{
    SizeHint hint = buildHint(base() | modifier::widthAtMost(40.0f));

    expectBand("width", hint.getWidth(), 10.0f, 40.0f, 40.0f);
    expectBand("height", hint.getHeightForWidth(100.0f), 20.0f, 60.0f, 120.0f);
}

TEST(sizeVocabulary, widthExactlyPinsTheBand)
{
    SizeHint hint = buildHint(base() | modifier::widthExactly(35.0f));

    expectBand("width", hint.getWidth(), 35.0f, 35.0f, 35.0f);
    expectBand("height", hint.getHeightForWidth(100.0f), 20.0f, 60.0f, 120.0f);
}

TEST(sizeVocabulary, preferWidthMovesTheNaturalWithinTheBand)
{
    SizeHint hint = buildHint(base() | modifier::preferWidth(80.0f));

    expectBand("width", hint.getWidth(), 10.0f, 80.0f, 100.0f);
}

TEST(sizeVocabulary, preferWidthClampsIntoTheBand)
{
    SizeHint hint = buildHint(base() | modifier::preferWidth(200.0f));

    expectBand("width", hint.getWidth(), 10.0f, 100.0f, 100.0f);
}

TEST(sizeVocabulary, heightModifiersTouchOnlyHeight)
{
    SizeHint atLeast = buildHint(base() | modifier::heightAtLeast(90.0f));
    expectBand("height", atLeast.getHeightForWidth(100.0f), 90.0f, 90.0f, 120.0f);
    expectBand("width", atLeast.getWidth(), 10.0f, 50.0f, 100.0f);

    SizeHint atMost = buildHint(base() | modifier::heightAtMost(40.0f));
    expectBand("height", atMost.getHeightForWidth(100.0f), 20.0f, 40.0f, 40.0f);

    SizeHint exactly = buildHint(base() | modifier::heightExactly(55.0f));
    expectBand("height", exactly.getHeightForWidth(100.0f), 55.0f, 55.0f, 55.0f);

    SizeHint prefer = buildHint(base() | modifier::preferHeight(30.0f));
    expectBand("height", prefer.getHeightForWidth(100.0f), 20.0f, 30.0f, 120.0f);
}

TEST(sizeVocabulary, bothAxisModifiersActOnBothBands)
{
    SizeHint atLeast = buildHint(base()
            | modifier::sizeAtLeast(avg::Vector2f(30.0f, 90.0f)));
    expectBand("width", atLeast.getWidth(), 30.0f, 50.0f, 100.0f);
    expectBand("height", atLeast.getHeightForWidth(100.0f), 90.0f, 90.0f, 120.0f);

    SizeHint atMost = buildHint(base()
            | modifier::sizeAtMost(avg::Vector2f(40.0f, 40.0f)));
    expectBand("width", atMost.getWidth(), 10.0f, 40.0f, 40.0f);
    expectBand("height", atMost.getHeightForWidth(100.0f), 20.0f, 40.0f, 40.0f);

    SizeHint exact = buildHint(base()
            | modifier::exactSize(avg::Vector2f(35.0f, 45.0f)));
    expectBand("width", exact.getWidth(), 35.0f, 35.0f, 35.0f);
    expectBand("height", exact.getHeightForWidth(100.0f), 45.0f, 45.0f, 45.0f);
}

// Add-only composition: two atLeast bounds keep the larger, and the result does
// not depend on the order they are applied.
TEST(sizeVocabulary, widthAtLeastComposesOrderIndependently)
{
    SizeHint forward = buildHint(base()
            | modifier::widthAtLeast(30.0f)
            | modifier::widthAtLeast(70.0f));

    SizeHint backward = buildHint(base()
            | modifier::widthAtLeast(70.0f)
            | modifier::widthAtLeast(30.0f));

    expectBand("forward", forward.getWidth(), 70.0f, 70.0f, 100.0f);
    expectBand("backward", backward.getWidth(), 70.0f, 70.0f, 100.0f);
}

// A minimum and a maximum stack: the band is tightened from both ends.
TEST(sizeVocabulary, atLeastAndAtMostTightenFromBothEnds)
{
    SizeHint hint = buildHint(base()
            | modifier::widthAtLeast(30.0f)
            | modifier::widthAtMost(70.0f));

    expectBand("width", hint.getWidth(), 30.0f, 50.0f, 70.0f);
}

// growWeight makes a widget a filler by writing a positive grow weight onto both
// axes, and leaves the size bands themselves untouched.
TEST(sizeVocabulary, growWeightSetsFillerWeightOnBothAxes)
{
    SizeHint hint = buildHint(base() | modifier::growWeight(2.0f));

    EXPECT_FLOAT_EQ(2.0f, hint.getWidth().extent.grow);
    EXPECT_FLOAT_EQ(2.0f, hint.getHeightForWidth(100.0f).extent.grow);

    expectBand("width", hint.getWidth(), 10.0f, 50.0f, 100.0f);
    expectBand("height", hint.getHeightForWidth(100.0f), 20.0f, 60.0f, 120.0f);
}
