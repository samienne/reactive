#include <reactive/signal/constant.h>
#include <reactive/signal/erasetype.h>
#include <reactive/signal/map.h>
#include <reactive/signal/input.h>
#include <reactive/signal/update.h>
#include <reactive/signal.h>

#include <reactive/stream/collect.h>
#include <reactive/stream/pipe.h>

#include <gtest/gtest.h>

#include <iostream>
#include <string>
#include <cmath>
#include <chrono>
#include <fstream>

using namespace reactive;
using us = std::chrono::microseconds;

TEST(SignalTypeReduction, signalTypeReduction)
{
    int count = 0;
    auto add = [&count](int l, int r)
    {
        ++count;
        return l + r;
    };

    auto s1 = signal::constant(10);
    auto s2 = signal::input(20);

    auto s3 = signal::map(add, std::move(s1), s2.signal);

    auto s4 = signal::map(add, std::move(s3), s2.signal);

    Signal<int> sig = std::move(s4);

    bool called = false;
    auto connection = sig.observe([&called]()
            {
                called = true;
            });

    EXPECT_FALSE(called);

    EXPECT_EQ(0, count);
    EXPECT_EQ(50, sig.evaluate());
    EXPECT_EQ(2, count);

    s2.handle.set(5);
    signal::update(sig, {1, us(0)});
    EXPECT_TRUE(called);

    EXPECT_TRUE(sig.hasChanged());
    EXPECT_EQ(2, count);

    EXPECT_EQ(20, sig.evaluate());
    EXPECT_EQ(4, count);
}

/*TEST(SignalTypeReduction, signalTypeReductionFail)
{
    Signal<int> s1;

    EXPECT_THROW(s1.evaluate(), std::runtime_error);
    EXPECT_THROW(s1.observe([]()
                {
                }), std::runtime_error);
    EXPECT_FALSE(s1.hasChanged());
}*/

TEST(SignalTypeReduction, multiSignalTypeReduction)
{
    int count = 0;
    auto add = [&count](int l, int r)
    {
        ++count;
        return l + r;
    };

    Signal<int> s1 = signal::constant(10);
    auto input = signal::input(20);
    Signal<int> s2 = btl::clone(input.signal);

    Signal<int> s3 = signal::map(add, s1.clone(), s2.clone());

    Signal<int> s4 = signal::map(add, s3.clone(), s2.clone());

    Signal<int> sig = s4.clone();

    bool called = false;
    auto connection = sig.observe([&called]()
            {
                called = true;
            });

    EXPECT_FALSE(called);

    EXPECT_EQ(0, count);
    EXPECT_EQ(50, sig.evaluate());
    EXPECT_EQ(2, count);

    input.handle.set(5);
    signal::update(sig, {1, us(0)});
    EXPECT_TRUE(called);

    EXPECT_TRUE(sig.hasChanged());
    EXPECT_EQ(2, count);

    EXPECT_EQ(20, sig.evaluate());
    EXPECT_EQ(4, count);
}

TEST(SignalTypeReduction, sharedSignal)
{
    auto add = [](int a, int b)
    {
        return a + b;
    };

    auto s1 = signal::input(10);
    Signal<int> s2(s1.signal.clone());

    auto s3 = signal::map(add, s2.clone(), s2.clone());

    EXPECT_EQ(20, s3.evaluate());

    s1.handle.set(20);
    signal::update(s3, {1, us(0)});

    EXPECT_TRUE(s3.hasChanged());

    EXPECT_EQ(40, s3.evaluate());
    EXPECT_TRUE(s3.hasChanged());

    signal::update(s3, {2, us(0)});
    EXPECT_FALSE(s3.hasChanged());
}

TEST(SignalTypeReduction, redundantTypeReduction)
{
    auto s1 = signal::input(10);
    Signal<int> s2(s1.signal.clone());

    auto s3 = signal::eraseType(s2.clone());

    std::ofstream of("redu.dot");
    of << s3.annotate().getDot() << std::endl;
}

TEST(SignalTypeReduction, convert)
{
    auto s1 = signal::constant<int>(200);

    static_assert(std::is_same<int const&, decltype(s1.evaluate())>::value, "");

    Signal<int> s2 = std::move(s1);

    EXPECT_EQ(200, s2.evaluate());

    Signal<int> s3 = btl::clone(s2);

    EXPECT_EQ(200, s3.evaluate());
}

