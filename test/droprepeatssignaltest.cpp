#include <reactive/signal/droprepeats.h>
#include <reactive/signal/input.h>
#include <reactive/signal/update.h>
#include <reactive/signal/signal.h>

#include <reactive/connection.h>

#include "signaltester.h"

#include <gtest/gtest.h>

#include <iostream>
#include <utility>
#include <string>
#include <cmath>
#include <thread>
#include <chrono>

using namespace reactive;
using us = std::chrono::microseconds;

TEST(DropRepeatsSignal, basic)
{
    auto i = signal::input(10);

    auto s1 = dropRepeats(i.signal);

    EXPECT_FALSE(s1.hasChanged());
    EXPECT_EQ(10, s1.evaluate());

    i.handle.set(10);
    signal::update(s1, {1, us(0)});

    EXPECT_FALSE(s1.hasChanged());
    EXPECT_EQ(10, s1.evaluate());

    i.handle.set(20);
    signal::update(s1, {2, us(0)});

    EXPECT_TRUE(s1.hasChanged());
    EXPECT_EQ(20, s1.evaluate());

    i.handle.set(20);
    signal::update(s1, {3, us(0)});

    EXPECT_FALSE(s1.hasChanged());
    EXPECT_EQ(20, s1.evaluate());
}

TEST(DropRepeatsSignal, tester)
{
    testSignal([](auto s)
    {
        return signal::dropRepeats(std::move(s));
    });
}

