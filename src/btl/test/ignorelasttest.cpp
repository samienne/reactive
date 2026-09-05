#include <btl/ignorelast.h>

#include <string>

#include "gtest/gtest.h"

TEST(IgnoreLast, ignoresTrailingArgFromZeroArgCallable)
{
    bool called = false;
    auto f = btl::ignoreLast([&called]() { called = true; });

    f(42);

    EXPECT_TRUE(called);
}

TEST(IgnoreLast, forwardsLeadingArgToOneArgCallable)
{
    int seen = 0;
    auto f = btl::ignoreLast([&seen](int x) { seen = x; });

    f(7, std::string("ignored"));

    EXPECT_EQ(7, seen);
}
