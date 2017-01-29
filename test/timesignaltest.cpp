#include <reactive/signal/dt.h>
#include <reactive/signal/time.h>
#include <reactive/signal/update.h>

#include <gtest/gtest.h>

#include <chrono>
#include <iostream>

using namespace reactive;
using us = std::chrono::microseconds;

TEST(DtSignal, basic)
{
    auto t = signal::dt();

    EXPECT_TRUE(t.hasChanged());
    EXPECT_EQ(us(0), t.evaluate());

    signal::update(t, {1, us(16667)});
    EXPECT_TRUE(t.hasChanged());
    EXPECT_EQ(us(16667), t.evaluate());
}

TEST(TimeSignal, basic)
{
    auto t = signal::time();

    EXPECT_TRUE(t.hasChanged());
    EXPECT_EQ(us(0), t.evaluate());

    signal::update(t, {1, us(16667)});
    EXPECT_TRUE(t.hasChanged());
    EXPECT_EQ(us(16667), t.evaluate());
}

