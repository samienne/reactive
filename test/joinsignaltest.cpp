#include "reactive/signal/join.h"
#include "reactive/signal/input.h"
#include "reactive/signal/map.h"
#include "reactive/signal/update.h"

#include <gtest/gtest.h>

using namespace reactive;

struct Test
{
    signal::InputHandle<int> h1;
    signal::InputHandle<signal2::SharedSignal<int>> h2;
    signal2::Signal<signal2::SharedSignal<int>> s1;
    signal2::Signal<int> s2;
};

Test makeTest()
{
    auto i1 = signal::input(10);
    auto i2 = signal::input(signal::share(makeSignal(std::move(i1.signal))));

    auto s = signal::join(btl::clone(i2.signal));

    return Test{
        i1.handle,
        i2.handle,
        std::move(i2.signal),
        std::move(s)
    };
}

TEST(JoinSignal, isSignal)
{
    static_assert(IsSignal<signal::Join<
            signal::Constant<signal::Constant<int>>>>::value,
            "Join is not a signal");
}

TEST(JoinSignal, construct)
{
    auto t = makeTest();

    EXPECT_EQ(10, t.s2.evaluate());

    t.h1.set(20);
    signal::update(t.s2, {1, std::chrono::milliseconds(1)});

    EXPECT_TRUE(t.s2.hasChanged());
    EXPECT_EQ(20, t.s2.evaluate());

    signal::update(t.s2, {2, std::chrono::milliseconds(1)});
    EXPECT_FALSE(t.s2.hasChanged());
    EXPECT_EQ(20, t.s2.evaluate());

    t.h1.set(30);
    signal::update(t.s2, {3, std::chrono::milliseconds(1)});

    EXPECT_TRUE(t.s2.hasChanged());
    EXPECT_EQ(30, t.s2.evaluate());

    signal::update(t.s2, {4, std::chrono::milliseconds(1)});
    EXPECT_FALSE(t.s2.hasChanged());
    EXPECT_EQ(30, t.s2.evaluate());
}

TEST(JoinSignal, changeInner)
{
    auto i1 = signal::input(10);

    auto s1 = std::move(i1.signal);

    auto i2 = signal::input(std::move(s1));
    auto s2 = std::move(i2.signal);

    auto s3 = signal::join(std::move(s2));

    auto i3 = signal::input(30);
    auto s4 = std::move(i3.signal);

    EXPECT_FALSE(s3.hasChanged());
    EXPECT_EQ(10, s3.evaluate());

    i2.handle.set(std::move(s4));

    signal::update(s3, {1, std::chrono::milliseconds(1)});

    EXPECT_TRUE(s3.hasChanged());
    EXPECT_EQ(30, s3.evaluate());
}

