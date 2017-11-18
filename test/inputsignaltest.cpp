#include <reactive/signal/tee.h>
#include <reactive/signal/constant.h>
#include <reactive/signal/input.h>
#include <reactive/signal/update.h>
#include <reactive/signal.h>
#include <reactive/connection.h>

#include <gtest/gtest.h>

#include <iostream>
#include <utility>
#include <string>
#include <cmath>
#include <thread>

using namespace reactive;
using std::chrono::microseconds;

TEST(InputSignal, Input)
{
    auto input = signal::input(10);

    EXPECT_EQ(10, input.signal.evaluate());

    input.handle.set(20);
    signal::update(input.signal, {1, microseconds(0)});
    EXPECT_TRUE(input.signal.hasChanged());

    EXPECT_EQ(20, input.signal.evaluate());
    EXPECT_TRUE(input.signal.hasChanged());

    signal::update(input.signal, {2, microseconds(0)});
    EXPECT_FALSE(input.signal.hasChanged());
}

TEST(InputSignal, InputCopy)
{
    auto input = signal::input(10);

    EXPECT_EQ(10, input.signal.evaluate());

    auto input2 = btl::clone(input.signal);
    EXPECT_EQ(10, input2.evaluate());

    // Both inputs should change
    input.handle.set(20);
    signal::update(input.signal, {1, microseconds(0)});
    signal::update(input2, {1, microseconds(0)});
    EXPECT_TRUE(input.signal.hasChanged());
    EXPECT_TRUE(input2.hasChanged());

    EXPECT_EQ(20, input.signal.evaluate());

    // input2 should still be in "changed" state as we haven't evaluated it
    EXPECT_TRUE(input2.hasChanged());

    EXPECT_EQ(20, input2.evaluate());

    EXPECT_TRUE(input.signal.hasChanged());
    EXPECT_TRUE(input2.hasChanged());

    signal::update(input.signal, {2, microseconds(0)});
    signal::update(input2, {2, microseconds(0)});

    EXPECT_FALSE(input.signal.hasChanged());
    EXPECT_FALSE(input2.hasChanged());
}

TEST(InputSignal, InputObserve)
{
    auto input = signal::input(10);

    EXPECT_EQ(10, input.signal.evaluate());

    int callCount = 0;
    auto connection = input.signal.observe([&callCount]()
            {
                ++callCount;
            });

    EXPECT_EQ(0, callCount);

    input.handle.set(20);
    signal::update(input.signal, {1, microseconds(0)});

    EXPECT_EQ(1, callCount);

    EXPECT_EQ(20, input.signal.evaluate());

    input.handle.set(20);
    EXPECT_EQ(2, callCount);
    input.handle.set(20);
    EXPECT_EQ(3, callCount);
    input.handle.set(20);
    EXPECT_EQ(4, callCount);
    input.handle.set(20);
    EXPECT_EQ(5, callCount);
}

TEST(InputSignal, circularSimple)
{
    auto input = signal::input(10);

    auto s1 = signal::tee(std::move(input.signal), input.handle);

    EXPECT_FALSE(s1.hasChanged());

    EXPECT_EQ(10, s1.evaluate());
    signal::update(s1, {1, microseconds(0)});

    EXPECT_FALSE(s1.hasChanged());
}

TEST(InputSignal, circularNoChange)
{
    auto input = signal::input(10);

    auto add = [](int a, int b)
    {
        return a + b;
    };

    auto s1 = signal::map(add, btl::clone(input.signal), signal::constant(10));
    auto s2 = signal::tee(std::move(s1), input.handle);

    EXPECT_FALSE(s2.hasChanged());

    EXPECT_EQ(20, s2.evaluate());
    EXPECT_EQ(20, s2.evaluate());

    signal::update(s2, {1, microseconds(0)});

    EXPECT_TRUE(s2.hasChanged());
    EXPECT_EQ(30, s2.evaluate());

    signal::update(s2, {2, microseconds(0)});

    EXPECT_TRUE(s2.hasChanged());
    EXPECT_EQ(40, s2.evaluate());
}

TEST(InputSignal, circularChange)
{
    auto input1 = signal::input(10);
    auto input2 = signal::input(10);

    auto add = [](int a, int b)
    {
        return a + b;
    };

    auto s1 = signal::map(add, input1.signal.clone(), input2.signal.clone());
    auto s2 = signal::tee(std::move(s1), input1.handle);

    EXPECT_FALSE(s2.hasChanged());
    EXPECT_EQ(20, s2.evaluate());

    input2.handle.set(0);
    signal::update(s2, {1, microseconds(0)});
    EXPECT_FALSE(s2.hasChanged());
    EXPECT_EQ(20, s2.evaluate());
    EXPECT_FALSE(s2.hasChanged());

    input2.handle.set(30);
    signal::update(s2, {2, microseconds(0)});

    EXPECT_TRUE(s2.hasChanged());
    EXPECT_EQ(50, s2.evaluate());

    input2.handle.set(20);
    signal::update(s2, {3, microseconds(0)});

    EXPECT_TRUE(s2.hasChanged());
    EXPECT_EQ(70, s2.evaluate());
}

TEST(InputSignal, inputChained)
{
    auto input1 = signal::input(10);
    auto input2 = signal::input(10);

    auto sig1 = signal::share(Signal<int>(std::move(input1.signal)));
    input2.handle.set(signal::weak(sig1).signal());

    input1.handle.set(20);

    signal::update(input2.signal, {1, microseconds(0)});

    EXPECT_EQ(20, input2.signal.evaluate());
}

TEST(InputSignal, teeOrder1)
{
    // Tee is used to pass the value "downstream"
    auto input1 = signal::input(10);
    auto input2 = signal::input(10);

    auto s1 = signal::tee(std::move(input1.signal), input2.handle);

    auto s2 = signal::map([](int, int i2)
            {
                return i2;
            }, std::move(s1), std::move(input2.signal));

    input1.handle.set(20);
    signal::update(s2, {1, microseconds(0)});
    auto v = s2.evaluate();
    EXPECT_EQ(20, v);
}

TEST(InputSignal, teeOrder2)
{
    // Tee is used to pass the value "upstream"
    auto input1 = signal::input(10);
    auto input2 = signal::input(10);

    auto s1 = signal::map([](int, int i2)
            {
                return i2;
            }, std::move(input1.signal), std::move(input2.signal));

    auto s2 = signal::tee(std::move(s1), [](int i) { return i*2; },
            input2.handle);

    // Still the initial value
    EXPECT_EQ(10, s2.evaluate());

    signal::update(s2, {1, microseconds(0)});

    EXPECT_EQ(20, s2.evaluate());

    signal::update(s2, {2, microseconds(0)});
    EXPECT_EQ(40, s2.evaluate());
}

