#include <btl/dropparam.h>

#include <string>

#include "gtest/gtest.h"

TEST(DropParam, dropsTrailingArgFromZeroArgCallable)
{
    bool called = false;
    auto f = btl::dropParam([&called]() { called = true; });

    f(42);

    EXPECT_TRUE(called);
}

TEST(DropParam, forwardsLeadingArgToOneArgCallable)
{
    int seen = 0;
    auto f = btl::dropParam([&seen](int x) { seen = x; });

    f(7, std::string("ignored"));

    EXPECT_EQ(7, seen);
}
