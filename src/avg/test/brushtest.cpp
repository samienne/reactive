#include <avg/brush.h>
#include <avg/transform.h>
#include <avg/color.h>

#include <gtest/gtest.h>

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

