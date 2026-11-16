#include <avg/brush.h>
#include <avg/builtineffects.h>
#include <avg/color.h>
#include <avg/font.h>
#include <avg/pen.h>
#include <avg/textentry.h>
#include <avg/transform.h>

#include <gtest/gtest.h>

using namespace avg;

namespace
{
    struct Glass
    {
        Glass()
        {
            FrostedGlassOptions options;
            options.radius = 12.0f;
            FrostedGlassOptions doubledOptions = options;
            doubledOptions.radius = 24.0f;
            doubledOptions.highlightWidth *= 2.0f;

            brush = Brush(frostedGlass(options));
            doubled = Brush(frostedGlass(doubledOptions));
        }

        Brush brush;
        Brush doubled;
    };

    TextEntry makeEntry(std::optional<Brush> brush, std::optional<Pen> pen)
    {
        return TextEntry(Font("data/fonts/OpenSans-Regular.ttf", 0),
                Transform(Vector2f(5.0f, 4.0f), 20.0f), "Glass",
                std::move(brush), std::move(pen));
    }

    void expectDoubled(Glass const& glass, TextEntry const& entry)
    {
        EXPECT_EQ(Transform(Vector2f(10.0f, 8.0f), 40.0f),
                entry.getTransform());
        ASSERT_TRUE(entry.getBrush().has_value());
        EXPECT_EQ(glass.doubled, *entry.getBrush());
        ASSERT_TRUE(entry.getPen().has_value());
        EXPECT_EQ(glass.doubled, entry.getPen()->getBrush());
        EXPECT_EQ(6.0f, entry.getPen()->getWidth());
    }
} // anonymous namespace

TEST(TextEntry, scalingKeepsAndScalesTheBrushAndThePen)
{
    Glass const glass;
    TextEntry const entry = makeEntry(glass.brush, Pen(glass.brush, 3.0f));

    expectDoubled(glass, entry * 2.0f);

    TextEntry inPlace = entry;
    inPlace *= 2.0f;
    expectDoubled(glass, inPlace);
}

TEST(TextEntry, transformingScalesTheBrushAndThePen)
{
    Glass const glass;
    TextEntry const entry = makeEntry(glass.brush, Pen(glass.brush, 3.0f));

    expectDoubled(glass, scale(2.0f) * entry);
}

TEST(TextEntry, scalingLeavesAColourBrushAndAMissingPenAsTheyAre)
{
    Brush const colour(Color(1.0f, 0.0f, 0.0f));
    TextEntry const entry = makeEntry(colour, std::nullopt);

    for (TextEntry const& scaled : { entry * 2.0f, scale(2.0f) * entry })
    {
        ASSERT_TRUE(scaled.getBrush().has_value());
        EXPECT_EQ(colour, *scaled.getBrush());
        EXPECT_FALSE(scaled.getPen().has_value());
    }

    TextEntry const bare = makeEntry(std::nullopt, std::nullopt) * 2.0f;
    EXPECT_FALSE(bare.getBrush().has_value());
}

TEST(TextEntry, translatingLeavesTheBrushAndThePenAsTheyAre)
{
    Glass const glass;
    Pen const pen(glass.brush, 3.0f);
    TextEntry const entry = makeEntry(glass.brush, pen);

    TextEntry const moved = entry + Vector2f(1.0f, 2.0f);
    EXPECT_EQ(glass.brush, *moved.getBrush());
    EXPECT_EQ(pen, *moved.getPen());
}
