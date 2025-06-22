#include <avg/color.h>

#include <gtest/gtest.h>

#include <sstream>

using namespace avg;

TEST(Color, get)
{
    Color c{1.0f, 0.75f, 0.50f, 0.25f};

    EXPECT_EQ(1.0f, c.getRed());
    EXPECT_EQ(0.75f, c.getGreen());
    EXPECT_EQ(0.50f, c.getBlue());
    EXPECT_EQ(0.25f, c.getAlpha());

    auto a = std::array<float, 4>{{1.0f, 0.75f, 0.50f, 0.25f}};
    EXPECT_EQ(a, c.getArray());

    std::ostringstream ss;
    ss << c;
    EXPECT_EQ(std::string("Color(1, 0.75, 0.5, 0.25)"), ss.str());
}

