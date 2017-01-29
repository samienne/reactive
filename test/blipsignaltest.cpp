#include <reactive/signal/blip.h>
#include <reactive/signal/constant.h>
#include <reactive/signal/input.h>
#include <reactive/signal/update.h>

#include <reactive/signaltraits.h>

#include "signaltester.h"

#include <gtest/gtest.h>

#include <chrono>

using us = std::chrono::microseconds;

using namespace reactive;
using namespace reactive::signal;

static_assert(reactive::IsSignal<Blip<Constant<int>>>::value, "");

TEST(Blip, evaluate)
{
    auto i = input(std::string("test"));

    auto s = blip(std::move(i.signal));

    EXPECT_FALSE(s.hasChanged());
    EXPECT_EQ(btl::just(std::string("test")), s.evaluate());

    i.handle.set("test2");

    signal::update(s, FrameInfo(1, us(1000000)));

    EXPECT_TRUE(s.hasChanged());
    EXPECT_EQ(btl::just<std::string>("test2"), s.evaluate());

    signal::update(s, FrameInfo(2, us(1000000)));

    EXPECT_TRUE(s.hasChanged());
    EXPECT_EQ(btl::none, s.evaluate());

    signal::update(s, FrameInfo(3, us(1000000)));

    EXPECT_FALSE(s.hasChanged());
    EXPECT_EQ(btl::none, s.evaluate());
}

TEST(Blip, tester)
{
    testSignal([](auto s)
    {
        return signal::blip(std::move(s));
    });
}

