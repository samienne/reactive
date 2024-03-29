#include <avg/animated.h>

#include <avg/curve/curves.h>

#include <chrono>
#include <gtest/gtest.h>

using namespace avg;

TEST(Animated, nonAnimatedValue)
{
    Animated<float> b(1.0f);
    EXPECT_FLOAT_EQ(1.0f, b.getValue(std::chrono::milliseconds(100)));
}

TEST(Animated, nonAnimatedUpdate)
{
    Animated<float> a(0.0f);
    Animated<float> b(1.0f);
    Animated<float> c = a.updated(b, std::nullopt, std::chrono::milliseconds(50));

    EXPECT_FLOAT_EQ(1.0f, c.getValue(std::chrono::milliseconds(0)));
    EXPECT_FLOAT_EQ(1.0f, c.getValue(std::chrono::milliseconds(50)));
    EXPECT_FLOAT_EQ(1.0f, c.getValue(std::chrono::milliseconds(100)));
    EXPECT_FLOAT_EQ(1.0f, c.getValue(std::chrono::milliseconds(200)));
}

TEST(Animated, simpleUpdate)
{
    Animated<float> a(0.0f);
    Animated<float> b(1.0f);

    AnimationOptions options {
        std::chrono::milliseconds(100),
        curve::linear
    };

    Animated<float> c = a.updated(b, options, std::chrono::milliseconds(0));
    EXPECT_FLOAT_EQ(0.0f, c.getValue(std::chrono::milliseconds(0)));
    EXPECT_FLOAT_EQ(0.5f, c.getValue(std::chrono::milliseconds(50)));
    EXPECT_FLOAT_EQ(1.0f, c.getValue(std::chrono::milliseconds(100)));
    EXPECT_FLOAT_EQ(1.0f, c.getValue(std::chrono::milliseconds(200)));
}

TEST(Animated, doubleUpdate)
{
    Animated<float> a(0.0f);
    Animated<float> b(1.0f);
    Animated<float> c(1.0f);

    AnimationOptions options {
        std::chrono::milliseconds(100),
        curve::linear
    };

    Animated<float> d = a.updated(b, options, std::chrono::milliseconds(0));

    EXPECT_FLOAT_EQ(0.0f, d.getValue(std::chrono::milliseconds(0)));
    EXPECT_FLOAT_EQ(0.5f, d.getValue(std::chrono::milliseconds(50)));
    EXPECT_FLOAT_EQ(1.0f, d.getValue(std::chrono::milliseconds(100)));
    EXPECT_FLOAT_EQ(1.0f, d.getValue(std::chrono::milliseconds(200)));

    // Updating to same value while transitioning should have no effect
    Animated<float> e = d.updated(c, options, std::chrono::milliseconds(50));

    EXPECT_FLOAT_EQ(0.0f, e.getValue(std::chrono::milliseconds(0)));
    EXPECT_FLOAT_EQ(0.5f, e.getValue(std::chrono::milliseconds(50)));
    EXPECT_FLOAT_EQ(1.0f, e.getValue(std::chrono::milliseconds(100)));
    EXPECT_FLOAT_EQ(1.0f, e.getValue(std::chrono::milliseconds(200)));

    // Updating with time point after the transition has happened with the
    // same value should do nothing.
    Animated<float> f = d.updated(c, options, std::chrono::milliseconds(150));
    //EXPECT_FLOAT_EQ(1.0f, e.getValue(std::chrono::milliseconds(0)));
    //EXPECT_FLOAT_EQ(1.0f, e.getValue(std::chrono::milliseconds(50)));
    EXPECT_FLOAT_EQ(0.0f, e.getValue(std::chrono::milliseconds(0)));
    EXPECT_FLOAT_EQ(0.5f, e.getValue(std::chrono::milliseconds(50)));
    EXPECT_FLOAT_EQ(1.0f, e.getValue(std::chrono::milliseconds(100)));
    EXPECT_FLOAT_EQ(1.0f, e.getValue(std::chrono::milliseconds(150)));
    EXPECT_FLOAT_EQ(1.0f, e.getValue(std::chrono::milliseconds(200)));
    EXPECT_FLOAT_EQ(1.0f, e.getValue(std::chrono::milliseconds(300)));

    // There should still be a single keyframe from previous transition
    EXPECT_EQ(1, f.getKeyFrames().size());
}

TEST(Animated, updateToNewValueWhileTransitioning)
{
    Animated<float> a(0.0f);
    Animated<float> b(1.0f);
    Animated<float> c(1.5f);

    AnimationOptions options {
        std::chrono::milliseconds(100),
        curve::linear
    };

    Animated<float> d = a.updated(b, options, std::chrono::milliseconds(0));
    Animated<float> e = d.updated(c, options, std::chrono::milliseconds(50));

    EXPECT_FLOAT_EQ(0.5f, e.getValue(std::chrono::milliseconds(0)));
    EXPECT_FLOAT_EQ(0.5f, e.getValue(std::chrono::milliseconds(50)));
    EXPECT_FLOAT_EQ(1.0f, e.getValue(std::chrono::milliseconds(100)));
    EXPECT_FLOAT_EQ(1.5f, e.getValue(std::chrono::milliseconds(200)));
}

TEST(Animated, redundantUpdateWithKeyframes)
{
    RepeatMode mode = RepeatMode::normal;

    Animated<float> a(1.0f);
    Animated<float> b(0.0f, std::chrono::milliseconds(0), {
            { 0.5f, curve::linear, std::chrono::milliseconds(100), mode, 1 },
            { 1.0f, curve::linear, std::chrono::milliseconds(400), mode, 1 }
            });

    AnimationOptions options {
        std::chrono::milliseconds(100),
        curve::linear
    };

    Animated<float> c = a.updated(b, options, std::chrono::milliseconds(0));

    EXPECT_FLOAT_EQ(1.0f, c.getValue(std::chrono::milliseconds(0)));
    EXPECT_FLOAT_EQ(0.0f, c.getValue(std::chrono::milliseconds(100)));
    EXPECT_FLOAT_EQ(0.5f, c.getValue(std::chrono::milliseconds(200)));
    EXPECT_FLOAT_EQ(0.75f, c.getValue(std::chrono::milliseconds(400)));
    EXPECT_FLOAT_EQ(1.0f, c.getValue(std::chrono::milliseconds(600)));
    EXPECT_FLOAT_EQ(1.0f, c.getValue(std::chrono::milliseconds(700)));

    // This should have no impact, thus test for the same values
    c = c.updated(b, options, std::chrono::milliseconds(0));

    EXPECT_FLOAT_EQ(1.0f, c.getValue(std::chrono::milliseconds(0)));
    EXPECT_FLOAT_EQ(0.0f, c.getValue(std::chrono::milliseconds(100)));
    EXPECT_FLOAT_EQ(0.5f, c.getValue(std::chrono::milliseconds(200)));
    EXPECT_FLOAT_EQ(0.75f, c.getValue(std::chrono::milliseconds(400)));
    EXPECT_FLOAT_EQ(1.0f, c.getValue(std::chrono::milliseconds(600)));
    EXPECT_FLOAT_EQ(1.0f, c.getValue(std::chrono::milliseconds(700)));

    // This should have no impact, thus test for the same values
    c = c.updated(b, options, std::chrono::milliseconds(400));

    EXPECT_FLOAT_EQ(1.0f, c.getValue(std::chrono::milliseconds(0)));
    EXPECT_FLOAT_EQ(0.0f, c.getValue(std::chrono::milliseconds(100)));
    EXPECT_FLOAT_EQ(0.5f, c.getValue(std::chrono::milliseconds(200)));
    EXPECT_FLOAT_EQ(0.75f, c.getValue(std::chrono::milliseconds(400)));
    EXPECT_FLOAT_EQ(1.0f, c.getValue(std::chrono::milliseconds(600)));
    EXPECT_FLOAT_EQ(1.0f, c.getValue(std::chrono::milliseconds(700)));
}

TEST(Animated, repeat)
{
    Animated<float> a(1.0f, std::chrono::milliseconds(100), {
            { 2.0f, curve::linear, std::chrono::milliseconds(200), RepeatMode::normal, 3 }
            });

    EXPECT_FALSE(a.isInfinite());

    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(0)));
    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(100)));
    //EXPECT_FLOAT_EQ(2.0f, a.getValue(std::chrono::milliseconds(300)));
    EXPECT_FLOAT_EQ(1.5f, a.getValue(std::chrono::milliseconds(400)));
    //EXPECT_FLOAT_EQ(2.0f, a.getValue(std::chrono::milliseconds(500)));
    EXPECT_FLOAT_EQ(1.5f, a.getValue(std::chrono::milliseconds(600)));
    EXPECT_FLOAT_EQ(2.0f, a.getValue(std::chrono::milliseconds(700)));
    EXPECT_FLOAT_EQ(2.0f, a.getValue(std::chrono::milliseconds(800)));
}

TEST(Animated, repeatReverse)
{
    Animated<float> a(1.0f, std::chrono::milliseconds(100), {
            { 2.0f, curve::linear, std::chrono::milliseconds(200), RepeatMode::reverse, 4 }
            });

    EXPECT_FALSE(a.isInfinite());

    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(0)));
    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(100)));
    EXPECT_FLOAT_EQ(2.0f, a.getValue(std::chrono::milliseconds(300)));
    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(500)));
    EXPECT_FLOAT_EQ(2.0f, a.getValue(std::chrono::milliseconds(700)));

    EXPECT_FLOAT_EQ(2.0f, a.getValue(std::chrono::milliseconds(900)));
    EXPECT_FLOAT_EQ(2.0f, a.getValue(std::chrono::milliseconds(1100)));
    EXPECT_FLOAT_EQ(2.0f, a.getValue(std::chrono::milliseconds(1300)));
}

TEST(Animated, repeatInifinite)
{
    Animated<float> a(1.0f, std::chrono::milliseconds(100), {
            { 2.0f, curve::linear, std::chrono::milliseconds(200), RepeatMode::normal, 0 }
            });

    EXPECT_TRUE(a.isInfinite());

    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(0)));
    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(100)));
    EXPECT_FLOAT_EQ(1.5f, a.getValue(std::chrono::milliseconds(200)));
    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(300)));
    EXPECT_FLOAT_EQ(1.5f, a.getValue(std::chrono::milliseconds(400)));
    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(500)));
    EXPECT_FLOAT_EQ(1.5f, a.getValue(std::chrono::milliseconds(600)));
    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(700)));
    EXPECT_FLOAT_EQ(1.5f, a.getValue(std::chrono::milliseconds(800)));

    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(900)));
    EXPECT_FLOAT_EQ(1.5f, a.getValue(std::chrono::milliseconds(1000)));
    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(1100)));
    EXPECT_FLOAT_EQ(1.5f, a.getValue(std::chrono::milliseconds(1200)));
    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(1300)));
    EXPECT_FLOAT_EQ(1.5f, a.getValue(std::chrono::milliseconds(1400)));
    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(1500)));
    EXPECT_FLOAT_EQ(1.5f, a.getValue(std::chrono::milliseconds(1600)));
}


TEST(Animated, repeatReverseInifinite)
{
    Animated<float> a(1.0f, std::chrono::milliseconds(100), {
            { 2.0f, curve::linear, std::chrono::milliseconds(200), RepeatMode::reverse, 0 }
            });

    EXPECT_TRUE(a.isInfinite());

    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(0)));
    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(100)));
    EXPECT_FLOAT_EQ(2.0f, a.getValue(std::chrono::milliseconds(300)));
    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(500)));
    EXPECT_FLOAT_EQ(2.0f, a.getValue(std::chrono::milliseconds(700)));

    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(900)));
    EXPECT_FLOAT_EQ(2.0f, a.getValue(std::chrono::milliseconds(1100)));
    EXPECT_FLOAT_EQ(1.0f, a.getValue(std::chrono::milliseconds(1300)));
    EXPECT_FLOAT_EQ(2.0f, a.getValue(std::chrono::milliseconds(1500)));
}


TEST(Animated, repeatReverseInifiniteUpdate)
{
    Animated<float> a(1.0f, std::chrono::milliseconds(0), {
            { 2.0f, curve::linear, std::chrono::milliseconds(200), RepeatMode::reverse, 0 }
            });

    Animated<float> b(3.0f);

    Animated<float> c = a.updated(b, std::nullopt, std::chrono::milliseconds(1100));

    EXPECT_FLOAT_EQ(3.0f, c.getValue(std::chrono::milliseconds(1100)));
    EXPECT_FLOAT_EQ(3.0f, c.getValue(std::chrono::milliseconds(1200)));

}
