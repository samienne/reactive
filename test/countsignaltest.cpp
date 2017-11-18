#include <reactive/signal/input.h>
#include <reactive/signal/map.h>
#include <reactive/signal/count.h>
#include <reactive/signal/constant.h>
#include <reactive/signal/update.h>
#include <reactive/signal.h>

#include <gtest/gtest.h>

#include <iostream>
#include <utility>
#include <string>
#include <cmath>
#include <thread>
#include <chrono>

using namespace reactive;
using us = std::chrono::microseconds;

TEST(CountSignal, count)
{
    auto s1 = signal::input(10);
    auto s2 = signal::count(s1.signal);

    // Initial value should be 0
    EXPECT_EQ(0, s2.evaluate());

    // No changed thus the value should still be 0
    EXPECT_EQ(0, s2.evaluate());

    s1.handle.set(20);
    signal::update(s2, {1, us(0)});
    EXPECT_EQ(1, s2.evaluate());

    s1.handle.set(20);
    signal::update(s2, {2, us(0)});
    EXPECT_EQ(2, s2.evaluate());

    // Setting the input twice should only increment once
    s1.handle.set(30);
    s1.handle.set(40);
    signal::update(s2, {3, us(0)});
    EXPECT_EQ(3, s2.evaluate());
}

TEST(CountSignal, countLifted)
{
    auto add = [](int a, int b)
    {
        return a + b;
    };

    auto s1 = signal::input(10);
    auto s2 = signal::input(20);

    auto s3 = signal::map(add, s1.signal, s2.signal);

    auto s4 = signal::count(std::move(s3));

    EXPECT_EQ(0, s4.evaluate());

    // Setting both lifted signals should increment just once
    s1.handle.set(20);
    s2.handle.set(10);
    signal::update(s4, {1, us(0)});
    EXPECT_EQ(1, s4.evaluate());

    s1.handle.set(10);
    signal::update(s4, {2, us(0)});
    EXPECT_EQ(2, s4.evaluate());

    s2.handle.set(20);
    signal::update(s4, {3, us(0)});
    EXPECT_EQ(3, s4.evaluate());
}

