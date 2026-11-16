#include "remote/screenshot.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

using namespace bqui::remote;

namespace
{
    void expectRect(LogicalRect const& expected, LogicalRect const& actual)
    {
        EXPECT_FLOAT_EQ(expected.x, actual.x);
        EXPECT_FLOAT_EQ(expected.y, actual.y);
        EXPECT_FLOAT_EQ(expected.width, actual.width);
        EXPECT_FLOAT_EQ(expected.height, actual.height);
    }

    // Each pixel's red is its column, green its row from the top, blue 7 and
    // alpha 99.
    ase::ReadbackImage gradient(int width, int height)
    {
        ase::ReadbackImage image;
        image.size = ase::Vector2i(width, height);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                image.pixels.insert(image.pixels.end(), {
                        static_cast<std::uint8_t>(x),
                        static_cast<std::uint8_t>(y), 7, 99 });
            }
        }
        return image;
    }
} // namespace

TEST(screenshot, plansTheWholeFrameWithoutARegion)
{
    auto plan = planScreenshot(ase::Vector2i(200, 100), 2.0f, std::nullopt,
            1.0f);

    ASSERT_TRUE(plan);
    EXPECT_EQ(ase::Vector2i(0, 0), plan->origin);
    EXPECT_EQ(ase::Vector2i(200, 100), plan->size);
    EXPECT_EQ(ase::Vector2i(200, 100), plan->outputSize);
    expectRect({ 0.0f, 0.0f, 100.0f, 50.0f }, plan->captured);
}

TEST(screenshot, mapsABottomLeftRegionToRowsFromTheTop)
{
    auto plan = planScreenshot(ase::Vector2i(200, 100), 2.0f,
            LogicalRect{ 10.0f, 5.0f, 20.0f, 10.0f }, 1.0f);

    ASSERT_TRUE(plan);
    EXPECT_EQ(ase::Vector2i(20, 70), plan->origin);
    EXPECT_EQ(ase::Vector2i(40, 20), plan->size);
    expectRect({ 10.0f, 5.0f, 20.0f, 10.0f }, plan->captured);
}

TEST(screenshot, clampsTheRegionToTheFrame)
{
    auto plan = planScreenshot(ase::Vector2i(100, 50), 1.0f,
            LogicalRect{ -10.0f, -10.0f, 30.0f, 20.0f }, 1.0f);

    ASSERT_TRUE(plan);
    EXPECT_EQ(ase::Vector2i(0, 40), plan->origin);
    EXPECT_EQ(ase::Vector2i(20, 10), plan->size);
    expectRect({ 0.0f, 0.0f, 20.0f, 10.0f }, plan->captured);

    auto beyond = planScreenshot(ase::Vector2i(100, 50), 1.0f,
            LogicalRect{ 90.0f, 40.0f, 50.0f, 50.0f }, 1.0f);
    ASSERT_TRUE(beyond);
    expectRect({ 90.0f, 40.0f, 10.0f, 10.0f }, beyond->captured);
}

TEST(screenshot, growsAFractionalRegionToWholePixels)
{
    auto plan = planScreenshot(ase::Vector2i(100, 50), 1.0f,
            LogicalRect{ 0.5f, 0.5f, 1.0f, 1.0f }, 1.0f);

    ASSERT_TRUE(plan);
    EXPECT_EQ(ase::Vector2i(2, 2), plan->size);
    expectRect({ 0.0f, 0.0f, 2.0f, 2.0f }, plan->captured);
}

TEST(screenshot, plansNothingForARegionOutsideTheFrame)
{
    EXPECT_FALSE(planScreenshot(ase::Vector2i(100, 50), 1.0f,
                LogicalRect{ 100.0f, 0.0f, 10.0f, 10.0f }, 1.0f));
    EXPECT_FALSE(planScreenshot(ase::Vector2i(100, 50), 1.0f,
                LogicalRect{ -20.0f, 0.0f, 10.0f, 10.0f }, 1.0f));
    EXPECT_FALSE(planScreenshot(ase::Vector2i(0, 0), 1.0f, std::nullopt,
                1.0f));
}

TEST(screenshot, scalesTheOutputSizeAndKeepsAPixel)
{
    auto half = planScreenshot(ase::Vector2i(201, 100), 1.0f, std::nullopt,
            0.5f);
    ASSERT_TRUE(half);
    EXPECT_EQ(ase::Vector2i(101, 50), half->outputSize);

    auto tiny = planScreenshot(ase::Vector2i(201, 100), 1.0f, std::nullopt,
            0.001f);
    ASSERT_TRUE(tiny);
    EXPECT_EQ(ase::Vector2i(1, 1), tiny->outputSize);
}

TEST(screenshot, cropsAndDropsAlpha)
{
    auto image = gradient(8, 6);
    auto plan = planScreenshot(image.size, 1.0f,
            LogicalRect{ 2.0f, 1.0f, 3.0f, 2.0f }, 1.0f);
    ASSERT_TRUE(plan);

    auto rgb = renderScreenshot(image, *plan);

    // Bottom-left (2, 1) with height 2 is rows 3 and 4 from the top.
    std::vector<std::uint8_t> expected{
        2, 3, 7, 3, 3, 7, 4, 3, 7,
        2, 4, 7, 3, 4, 7, 4, 4, 7,
    };
    EXPECT_EQ(expected, rgb);
}

TEST(screenshot, downscalesByAveragingInLinearLight)
{
    ase::ReadbackImage image;
    image.size = ase::Vector2i(2, 2);
    image.pixels = {
        0, 0, 0, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 0, 0, 0, 255,
    };

    auto plan = planScreenshot(image.size, 1.0f, std::nullopt, 0.5f);
    ASSERT_TRUE(plan);
    ASSERT_EQ(ase::Vector2i(1, 1), plan->outputSize);

    // Half black, half white is linear 0.5, sRGB-encoded as 188 rather than
    // the 128 a plain byte average gives.
    EXPECT_EQ((std::vector<std::uint8_t>{ 188, 188, 188 }),
            renderScreenshot(image, *plan));
}

TEST(screenshot, keepsAFlatColourWhenDownscalingUnevenly)
{
    ase::ReadbackImage image;
    image.size = ase::Vector2i(5, 3);
    for (int i = 0; i < 15; ++i)
        image.pixels.insert(image.pixels.end(), { 10, 120, 240, 255 });

    auto plan = planScreenshot(image.size, 1.0f, std::nullopt, 0.6f);
    ASSERT_TRUE(plan);
    ASSERT_EQ(ase::Vector2i(3, 2), plan->outputSize);

    auto rgb = renderScreenshot(image, *plan);
    ASSERT_EQ(18u, rgb.size());
    for (size_t i = 0; i < rgb.size(); i += 3)
    {
        EXPECT_EQ(10, rgb[i]);
        EXPECT_EQ(120, rgb[i + 1]);
        EXPECT_EQ(240, rgb[i + 2]);
    }
}

TEST(screenshot, rejectsAPlanThatDoesNotFitTheImage)
{
    auto image = gradient(4, 4);
    ScreenshotPlan plan;
    plan.origin = ase::Vector2i(2, 2);
    plan.size = ase::Vector2i(4, 4);
    plan.outputSize = plan.size;

    EXPECT_THROW(renderScreenshot(image, plan), std::invalid_argument);
}
