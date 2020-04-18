#include "signaltester.h"

#include <reactive/signal/update.h>
#include <reactive/signal/group.h>
#include <reactive/signal/input.h>
#include <reactive/signal/signal.h>
#include <reactive/signal/constant.h>

#include <gtest/gtest.h>

#include <string>

using namespace reactive::signal;
using us = std::chrono::microseconds;

struct Identity
{
    template <typename T>
    auto operator()(T&& a) const
    {
        return std::forward<decltype(a)>(a);
    }
};

static_assert(IsSignal<Map3<Identity, AnySignal<int>>>::value, "");

TEST(Signal, map3)
{
    auto s = constant(10)
        .map([](int n)
                {
                    return std::to_string(n);
                })
        ;

    EXPECT_EQ("10", s.evaluate());
}

TEST(Signal, mapTester)
{
    testSignal([](auto s)
    {
        return std::move(s).map([](int i)
            {
                return i * 2;
            });
    });
}

TEST(Signal, mapInput)
{
    auto add = [](float l, float r)
    {
        return l + r;
    };

    auto s1 = constant(10);
    auto s2 = input(20);

    auto s3 = group(std::move(s1), std::move(s2.signal)).map(add);
    s3.evaluate();

    static_assert(IsSignal<decltype(s3)>::value, "");
    static_assert(std::is_same<SignalValueType<decltype(s3)>::type,
            float>::value, "");

    EXPECT_FALSE(s3.hasChanged());
    EXPECT_EQ(30, s3.evaluate());

    s2.handle.set(10);
    reactive::signal::update(s3, {1, us(0)});

    EXPECT_TRUE(s3.hasChanged());
    EXPECT_EQ(20, s3.evaluate());
    EXPECT_TRUE(s3.hasChanged());
    EXPECT_EQ(20, s3.evaluate());
    EXPECT_TRUE(s3.hasChanged());
}
