#include <avg/animated.h>

#include <chrono>
#include <gtest/gtest.h>

#include <iostream>

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
    Animated<float> a(1.0f);
    Animated<float> b(0.0f, std::chrono::milliseconds(0), {
            { 0.5f, curve::linear, std::chrono::milliseconds(100) },
            { 1.0f, curve::linear, std::chrono::milliseconds(400) }
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

