#include <arrange/arrange.h>

#include <gtest/gtest.h>

TEST(Smoke, LibraryLinks)
{
    arrange::Variable v{"smoke"};
    EXPECT_NE(v.id(), arrange::nullId);
    EXPECT_EQ(v.name(), "smoke");
}
