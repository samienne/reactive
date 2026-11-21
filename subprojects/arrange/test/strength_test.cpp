#include <arrange/strength.h>

#include <gtest/gtest.h>

namespace arrange
{

TEST(Strength, RequiredHasCanonicalValue)
{
    EXPECT_DOUBLE_EQ(Strength::required().value(), 1'001'001'000.0);
    EXPECT_TRUE(Strength::required().isRequired());
}

TEST(Strength, StrongHasCanonicalValue)
{
    EXPECT_DOUBLE_EQ(Strength::strong().value(), 1'000'000.0);
    EXPECT_FALSE(Strength::strong().isRequired());
}

TEST(Strength, MediumHasCanonicalValue)
{
    EXPECT_DOUBLE_EQ(Strength::medium().value(), 1'000.0);
}

TEST(Strength, WeakHasCanonicalValue)
{
    EXPECT_DOUBLE_EQ(Strength::weak().value(), 1.0);
}

TEST(Strength, WeightScalesLinearly)
{
    EXPECT_DOUBLE_EQ(Strength::strong(2.0).value(), 2'000'000.0);
    EXPECT_DOUBLE_EQ(Strength::medium(0.5).value(), 500.0);
    EXPECT_DOUBLE_EQ(Strength::weak(3.0).value(), 3.0);
}

TEST(Strength, Ordering)
{
    EXPECT_LT(Strength::weak(), Strength::medium());
    EXPECT_LT(Strength::medium(), Strength::strong());
    EXPECT_LT(Strength::strong(), Strength::required());
}

TEST(Strength, EqualityBetweenEquivalentStrengths)
{
    EXPECT_EQ(Strength::strong(), Strength::strong());
    EXPECT_EQ(Strength::strong(2.0), Strength::strong(2.0));
    EXPECT_NE(Strength::strong(), Strength::strong(2.0));
}

TEST(Strength, OnlyRequiredReturnsIsRequired)
{
    EXPECT_TRUE(Strength::required().isRequired());
    EXPECT_FALSE(Strength::strong(1000.0).isRequired())
        << "scaled strong should not cross into required";
    EXPECT_FALSE(Strength::medium().isRequired());
    EXPECT_FALSE(Strength::weak().isRequired());
}

TEST(Strength, CreatePacksComponents)
{
    // create(a, b, c, w) = w * (a*1e6 + b*1e3 + c)
    Strength s = Strength::create(1.0, 0.0, 0.0);
    EXPECT_DOUBLE_EQ(s.value(), 1'000'000.0);

    Strength t = Strength::create(0.0, 1.0, 0.0);
    EXPECT_DOUBLE_EQ(t.value(), 1'000.0);

    Strength u = Strength::create(0.0, 0.0, 1.0);
    EXPECT_DOUBLE_EQ(u.value(), 1.0);

    Strength mix = Strength::create(2.0, 3.0, 4.0);
    EXPECT_DOUBLE_EQ(mix.value(), 2'003'004.0);
}

TEST(Strength, CreateAppliesWeight)
{
    Strength s = Strength::create(1.0, 0.0, 0.0, 2.0);
    EXPECT_DOUBLE_EQ(s.value(), 2'000'000.0);
}

TEST(Strength, NegativeWeightClampedToZero)
{
    EXPECT_DOUBLE_EQ(Strength::strong(-1.0).value(), 0.0);
}

}  // namespace arrange
