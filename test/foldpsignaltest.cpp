#if 0
#include <reactive/signal/foldp.h>
#include <reactive/signal/input.h>
#include <reactive/signal/update.h>

#include "signaltester.h"

#include <gtest/gtest.h>

using namespace reactive;

TEST(foldp, simple)
{
    auto add = [](int a, int b)
    {
        return a + b;
    };

    auto i = signal::input(10);

    auto s1 = signal::foldp(add, 0, std::move(i.signal));

    EXPECT_EQ(0, s1.evaluate());
    EXPECT_FALSE(s1.hasChanged());

    i.handle.set(1);

    signal::update(s1, {1, std::chrono::milliseconds(0)});

    EXPECT_EQ(1, s1.evaluate());
    EXPECT_TRUE(s1.hasChanged());

    signal::update(s1, {2, std::chrono::milliseconds(0)});

    EXPECT_EQ(1, s1.evaluate());
    EXPECT_FALSE(s1.hasChanged());
}

TEST(foldp, tester)
{
    auto add = [](int a, int b)
    {
        return a + b;
    };

    testSignal([add](auto s)
    {
        return signal::foldp(add, 0, std::move(s));
    });
}

#endif
