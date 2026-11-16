#include <avg/brush.h>
#include <avg/builtineffects.h>
#include <avg/lerp.h>
#include <avg/pen.h>
#include <avg/transform.h>
#include <avg/color.h>

#include <btl/fnv1a.h>
#include <btl/uhash.h>

#include <gtest/gtest.h>

#include <sstream>

using namespace avg;

TEST(Brush, compare)
{
    Brush b1(Color(1.0, 1.0, 1.0, 1.0));
    Brush b2(Color(0.0, 0.0, 0.0, 1.0));
    auto b3 = b1;

    EXPECT_TRUE(b2 < b1);
    EXPECT_TRUE(b1 > b2);
    EXPECT_TRUE(b1 != b2);
    EXPECT_FALSE(b1 == b2);
    EXPECT_TRUE(b1 == b3);

    auto c = Color(1.0, 1.0, 1.0, 1.0);
    EXPECT_EQ(c, b1.getColor());
}

TEST(Brush, transform)
{
    Transform t;
    Brush b1(Color(1.0, 1.0, 1.0, 1.0));

    auto b2 = t * b1;
    auto b3 = b1 * 2.0f;
}


namespace
{
    Effect glassWithRadius(float radius)
    {
        FrostedGlassOptions options;
        options.radius = radius;
        return frostedGlass(options);
    }
} // anonymous namespace

TEST(Brush, holdsAColourOrAnEffect)
{
    Brush colour(Color(1.0f, 0.0f, 0.0f, 1.0f));
    Brush glass(glassWithRadius(24.0f));

    EXPECT_EQ(nullptr, colour.getEffect());
    ASSERT_NE(nullptr, glass.getEffect());
    EXPECT_EQ(glassWithRadius(24.0f), *glass.getEffect());
}

TEST(Brush, effectBrushesCompareByTheirEffects)
{
    Brush glass(glassWithRadius(24.0f));
    Brush sameGlass(glassWithRadius(24.0f));
    Brush otherGlass(glassWithRadius(8.0f));
    Brush colour(Color(1.0f, 1.0f, 1.0f, 1.0f));

    EXPECT_EQ(glass, sameGlass);
    EXPECT_NE(glass, otherGlass);
    EXPECT_NE(glass, colour);

    EXPECT_TRUE(colour < glass);
    EXPECT_FALSE(glass < colour);
    EXPECT_FALSE(glass < otherGlass);
    EXPECT_FALSE(otherGlass < glass);
}

TEST(Brush, equalEffectBrushesHashEqually)
{
    btl::uhash<btl::fnv1a> hash;

    EXPECT_EQ(hash(Brush(glassWithRadius(24.0f))),
            hash(Brush(glassWithRadius(24.0f))));
}

TEST(Brush, isSameKindPairsColoursAndEffectsOfOneFactory)
{
    Brush colour(Color(1.0f, 1.0f, 1.0f, 1.0f));
    Brush glass(glassWithRadius(24.0f));
    Brush dim(tint(Color(0.0f, 0.0f, 0.0f), 0.5f));

    EXPECT_TRUE(isSameKind(colour, Brush(Color())));
    EXPECT_TRUE(isSameKind(glass, Brush(glassWithRadius(8.0f))));
    EXPECT_FALSE(isSameKind(glass, dim));
    EXPECT_FALSE(isSameKind(colour, glass));
    EXPECT_FALSE(isSameKind(glass, colour));
}

TEST(Brush, lerpInterpolatesColours)
{
    Brush black(Color(0.0f, 0.0f, 0.0f, 1.0f));
    Brush white(Color(1.0f, 1.0f, 1.0f, 1.0f));

    EXPECT_EQ(Brush(Color(0.5f, 0.5f, 0.5f, 1.0f)), lerp(black, white, 0.5f));
}

TEST(Brush, lerpInterpolatesEffectsOfOneKind)
{
    Brush thin(glassWithRadius(8.0f));
    Brush thick(glassWithRadius(24.0f));

    EXPECT_EQ(Brush(glassWithRadius(16.0f)), lerp(thin, thick, 0.5f));
}

TEST(Brush, lerpSwitchesAtOnceBetweenUnlikeBrushes)
{
    Brush colour(Color(1.0f, 1.0f, 1.0f, 1.0f));
    Brush glass(glassWithRadius(24.0f));
    Brush dim(tint(Color(0.0f, 0.0f, 0.0f), 0.5f));

    EXPECT_EQ(glass, lerp(colour, glass, 0.0f));
    EXPECT_EQ(glass, lerp(colour, glass, 0.5f));
    EXPECT_EQ(colour, lerp(glass, colour, 0.0f));
    EXPECT_EQ(dim, lerp(glass, dim, 0.5f));
}

TEST(Brush, aPenCarriesAnEffectBrush)
{
    Pen pen(Brush(glassWithRadius(24.0f)), 2.0f);

    ASSERT_NE(nullptr, pen.getBrush().getEffect());
    EXPECT_EQ(glassWithRadius(24.0f), *pen.getBrush().getEffect());
}

TEST(Brush, printsAnEffectAsItself)
{
    Effect const effect = tint(Color(0.0f, 0.0f, 1.0f), 0.5f);

    std::ostringstream printedEffect;
    printedEffect << effect;
    EXPECT_EQ("Effect{1 passes}", printedEffect.str());

    std::ostringstream printedBrush;
    printedBrush << Brush(effect);
    EXPECT_EQ("Brush{Effect{1 passes}}", printedBrush.str());
}

TEST(Brush, scalingScalesAnEffectsSizes)
{
    FrostedGlassOptions options;
    options.radius = 12.0f;
    FrostedGlassOptions doubled = options;
    doubled.radius = 24.0f;
    doubled.highlightWidth *= 2.0f;

    Brush const glass(frostedGlass(options));
    EXPECT_EQ(Brush(frostedGlass(doubled)), glass * 2.0f);
    EXPECT_EQ(Brush(frostedGlass(doubled)), scale(2.0f) * glass);

    Brush const colour(Color(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(colour, colour * 2.0f);

    Pen const pen = Pen(glass, 1.0f, JOIN_MITER, END_CLOSEDPOLYGON) * 2.0f;
    EXPECT_EQ(Brush(frostedGlass(doubled)), pen.getBrush());
    EXPECT_EQ(JOIN_MITER, pen.getJoinType());
    EXPECT_EQ(END_CLOSEDPOLYGON, pen.getEndType());
    EXPECT_EQ(JOIN_MITER,
            (scale(2.0f) * Pen(glass, 1.0f, JOIN_MITER)).getJoinType());
}
