#include <ase/genericwindow.h>
#include <ase/vector.h>

#include <gtest/gtest.h>

TEST(genericWindow, sizesAgreeAtOneToOne)
{
    ase::GenericWindow window(ase::Vector2i(640, 480), 1.0f);
    EXPECT_EQ(ase::Vector2i(640, 480), window.getSize());
    EXPECT_EQ(ase::Vector2i(640, 480), window.getScaledSize());

    window.resize(ase::Vector2i(801, 601));
    EXPECT_EQ(ase::Vector2i(801, 601), window.getSize());
    EXPECT_EQ(ase::Vector2i(801, 601), window.getScaledSize());
}

TEST(genericWindow, keepsTheClientAreaInPixels)
{
    ase::GenericWindow window(ase::Vector2i(1000, 500), 1.5f);
    EXPECT_EQ(ase::Vector2i(1500, 750), window.getScaledSize());

    // 1501 / 1.5 rounds to 1001 logical, which scales back to 1501.5: the
    // pixel size must come from the platform, not from the logical size.
    window.resize(ase::Vector2i(1501, 801));
    EXPECT_EQ(ase::Vector2i(1501, 801), window.getScaledSize());
    EXPECT_EQ(ase::Vector2i(1001, 534), window.getSize());

    window.setScalingFactor(2.0f);
    EXPECT_EQ(ase::Vector2i(1501, 801), window.getScaledSize());
    EXPECT_EQ(ase::Vector2i(751, 401), window.getSize());
}
