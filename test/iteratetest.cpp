#include <reactive/stream/iterate.h>
#include <reactive/stream/pipe.h>

#include <reactive/signal/update.h>

#include <gtest/gtest.h>

#include <chrono>

using namespace reactive;
using namespace reactive::stream;

using us = std::chrono::microseconds;
using ms = std::chrono::milliseconds;

TEST(IterateSignal, iterate)
{
    auto p = pipe<int>();

    auto add = [](int a, int b)
    {
        return a + b;
    };

    auto f = [&]()
    {
        return iterate(add, signal::constant(10), std::move(p.stream));
    };

    //auto s1 = iterate(add, signal::constant(10), std::move(p.stream));
    auto s1 = f();
    static_assert(signal::CheckSignal<decltype(s1)>::value, "Iterate is not a signal");

    EXPECT_EQ(10, s1.evaluate());

    p.handle.push(1);
    EXPECT_FALSE(s1.hasChanged());
    signal::update(s1, signal::FrameInfo(1, us(0)));

    EXPECT_TRUE(s1.hasChanged());
    EXPECT_EQ(11, s1.evaluate());

    p.handle.push(2);
    p.handle.push(3);
    signal::update(s1, signal::FrameInfo(2, us(0)));

    EXPECT_EQ(16, s1.evaluate());

    p.handle.push(4);
    p.handle.push(5);
    p.handle.push(6);
    p.handle.push(7);
    signal::update(s1, signal::FrameInfo(3, us(0)));

    EXPECT_EQ(38, s1.evaluate());
}

TEST(IterateSignal, iterate2)
{
    auto add = [](int a, int b)
    {
        return a + b;
    };

    auto p = pipe<int>();

    auto s1 = iterate(add, signal::constant(10), std::move(p.stream));

    EXPECT_EQ(10, s1.evaluate());

    p.handle.push(10);
    signal::update(s1, signal::FrameInfo(1, us(0)));

    EXPECT_EQ(20, s1.evaluate());

    p.handle.push(10);
    signal::update(s1, signal::FrameInfo(2, us(0)));

    EXPECT_EQ(30, s1.evaluate());

    signal::update(s1, signal::FrameInfo(3, us(0)));

    EXPECT_EQ(30, s1.evaluate());

    signal::update(s1, signal::FrameInfo(4, us(0)));

    EXPECT_EQ(30, s1.evaluate());
}

TEST(IterateSignal, iterateSignalDifferentTypes)
{
    auto p = pipe<int>();

    auto step = [](int a, bool b)
    {
        if (b)
            return a + 1;
        else
            return a;
    };

    auto s1 = iterate(step, signal::constant(2), std::move(p.stream));

    EXPECT_EQ(2, s1.evaluate());

    p.handle.push(true);
    signal::update(s1, signal::FrameInfo(1, us(0)));

    EXPECT_EQ(3, s1.evaluate());

    p.handle.push(false);
    signal::update(s1, signal::FrameInfo(2, us(0)));

    EXPECT_EQ(3, s1.evaluate());

    p.handle.push(false);
    p.handle.push(true);
    p.handle.push(false);
    p.handle.push(false);
    signal::update(s1, signal::FrameInfo(3, us(0)));

    EXPECT_EQ(4, s1.evaluate());
}

TEST(IterateSignal, iterateCircular)
{
    auto add = [](int a, int b)
    {
        return a + b;
    };

    auto p = pipe<int>();
    auto i = signal::input(10);

    auto s2 = signal::tee(iterate(add, i.signal, std::move(p.stream)), i.handle);

    EXPECT_EQ(10, s2.evaluate());

    p.handle.push(20);

    signal::update(s2, signal::FrameInfo(1, us(0)));

    EXPECT_EQ(30, s2.evaluate());

    p.handle.push(30);

    signal::update(s2, signal::FrameInfo(2, us(0)));

    EXPECT_EQ(60, s2.evaluate());
}

TEST(IterateSignal, moveOnlyState)
{
    struct MoveOnly
    {
        std::unique_ptr<std::string> data;
    };

    auto state = MoveOnly{std::make_unique<std::string>("test")};

    auto events = stream::pipe<std::string>();

    auto s1 = stream::iterate(
            [](MoveOnly state, std::string event)
            {
                *state.data = event;
                return state;
            },
            std::move(state),
            std::move(events.stream)
            );

    EXPECT_EQ("test", *s1.evaluate().data);

    events.handle.push("test2");

    signal::update(s1, signal::FrameInfo(1, us(0)));

    EXPECT_EQ("test2", *s1.evaluate().data);
}

