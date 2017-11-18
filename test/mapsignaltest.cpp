#include <reactive/signal/share.h>
#include <reactive/signal/erasetype.h>
#include <reactive/signal/map.h>
#include <reactive/signal/cast.h>
#include <reactive/signal/input.h>
#include <reactive/signal/constant.h>
#include <reactive/signal/update.h>
#include <reactive/signal.h>

#include <btl/tuplemap.h>
#include <btl/apply.h>
#include <btl/tuplereduce.h>

#include "signaltester.h"

#include "gtest/gtest.h"

#include <iostream>
#include <string>
#include <cmath>
#include <chrono>
#include <fstream>

using namespace reactive;
using us = std::chrono::microseconds;

static_assert(IsSignal<
        signal::Map<
            signal::detail::MapBase, btl::Plus, signal::Constant<int>,
            signal::Constant<int>
            >
        >::value, "");

static_assert(std::is_same
        <
            int,
            SignalValueType<signal::Map<signal::detail::MapBase, btl::Plus,
                signal::Constant<int>, signal::Constant<int>>>::type
        >::value, "");


TEST(Map, sharedSignalAsParam)
{
    auto s1 = signal::share(signal::constant(10));
    auto s2 = signal::map([](int i) { return 2 * i; }, s1);
}

TEST(Map, tupleReduce)
{
    auto and_ = [](bool a, bool b) -> bool
    {
        return a && b;
    };

    bool b1 = btl::tuple_reduce(true, std::make_tuple(true, true, true), and_);
    EXPECT_TRUE(b1);

    bool b2 = btl::tuple_reduce(true, std::make_tuple(), and_);
    EXPECT_TRUE(b2);
}

TEST(Map, Partial)
{
    using namespace reactive::signal;
    auto f = [](int n, int m)
    {
        return n + m;
    };

    static_assert(btl::CanApply<decltype(f)(int, int)>::value, "");
    static_assert(!btl::CanApply<decltype(f)(int)>::value, "");

    btl::apply([](){}, std::tuple<>());

    auto v = btl::applyPartial(f, 10, 20);
    static_assert(std::is_same<int, decltype(v)>::value, "");
    EXPECT_EQ(30, v);
    auto g = btl::applyPartial(f);
    auto h = btl::applyPartial(f, 10);
    auto n = h(20);

    auto gs = eraseType(mapFunction(f));
    auto gss = gs.evaluate();
    auto v2 = gss(10, 20);

    Signal<std::function<int(int)>> s1 =
        signal::cast<std::function<int(int)>>(mapFunction(f, signal::constant(10)));

    //auto v1 = s1.evaluate();

    EXPECT_EQ(30, v2);
    EXPECT_EQ(30, n);
    EXPECT_EQ(30, g(10, 20));
    EXPECT_EQ(30, s1.evaluate()(20));
}

TEST(MapSignal, map)
{
    auto add = [](float l, float r)
    {
        return l + r;
    };

    auto s1 = signal::constant(10);
    auto s2 = signal::input(20);

    auto s3 = signal::map(add, std::move(s1), std::move(s2.signal));
    s3.evaluate();

    static_assert(IsSignal<decltype(s3)>::value, "");
    static_assert(std::is_same<SignalValueType<decltype(s3)>::type,
            float>::value, "");

    EXPECT_FALSE(s3.hasChanged());
    EXPECT_EQ(30, s3.evaluate());

    s2.handle.set(10);
    signal::update(s3, {1, us(0)});

    EXPECT_TRUE(s3.hasChanged());
    EXPECT_EQ(20, s3.evaluate());
    EXPECT_TRUE(s3.hasChanged());
    EXPECT_EQ(20, s3.evaluate());
    EXPECT_TRUE(s3.hasChanged());
}

TEST(MapSignal, mapMultiple)
{
    int count = 0;
    auto add = [&count](float l, float r)
    {
        ++count;
        return l + r;
    };

    auto s1 = signal::constant(10);
    auto s2 = signal::input(20);

    auto s3 = signal::map(add, std::move(s1), s2.signal.clone());

    auto s4 = signal::map(add, std::move(s3), s2.signal.clone());

    EXPECT_EQ(0, count);
    EXPECT_EQ(50, s4.evaluate());
    EXPECT_EQ(2, count);

    s2.handle.set(5);
    signal::update(s4, {1, us(0)});
    EXPECT_TRUE(s4.hasChanged());
    EXPECT_EQ(2, count);

    EXPECT_EQ(20, s4.evaluate());
    EXPECT_EQ(4, count);

    EXPECT_TRUE(s4.hasChanged());
}

TEST(MapSignal, mapMultipleObserve)
{
    int count = 0;
    auto add = [&count](int l, int r)
    {
        ++count;
        return l + r;
    };

    auto s1 = signal::constant(10);
    auto s2 = signal::input(20);

    auto s3 = signal::map(add, std::move(s1), s2.signal.clone());

    auto s4 = signal::map(add, std::move(s3), s2.signal.clone());

    bool called = false;
    auto connection = s4.observe([&called]()
            {
                called = true;
            });

    EXPECT_FALSE(called);

    EXPECT_EQ(0, count);
    EXPECT_EQ(50, s4.evaluate());
    EXPECT_EQ(2, count);

    s2.handle.set(5);
    signal::update(s4, {1, us(0)});
    EXPECT_TRUE(called);

    EXPECT_TRUE(s4.hasChanged());
    EXPECT_EQ(2, count);

    EXPECT_EQ(20, s4.evaluate());
    EXPECT_EQ(4, count);
}

TEST(MapSignal, mapMultipleObserve2)
{
    int count = 0;
    auto add = [&count](int l, int r)
    {
        ++count;
        return l + r;
    };

    auto s1 = signal::constant(10);
    auto s2 = signal::input(20);

    auto s3 = signal::map(add, std::move(s1), s2.signal.clone());

    auto s4 = signal::map(add, s3.clone(), s2.signal.clone());

    bool called = false;
    auto connection = s3.observe([&called]()
            {
                called = true;
            });

    EXPECT_FALSE(called);

    EXPECT_EQ(20, s2.signal.evaluate());

    s2.handle.set(5);
    signal::update(s4, {1, us(0)});
    signal::update(s2.signal, {1, us(0)});

    EXPECT_TRUE(s2.signal.hasChanged());

    EXPECT_EQ(5, s2.signal.evaluate());
}

TEST(MapSignal, mapTemporary)
{
    static_assert(btl::IsClonable<std::shared_ptr<int>>::value, "");
    auto i = signal::input(std::make_shared<int>(10));

    auto s1 = signal::map([](std::shared_ptr<int> i) -> std::shared_ptr<int>
            {
                return i;
            }, std::move(i.signal));

    static_assert(btl::IsClonable<decltype(s1)>::value, "");

    auto s2 = signal::map([](std::shared_ptr<int> const& i)
            -> std::shared_ptr<int> const&
            {
                return i;
            }, std::move(s1));

    EXPECT_EQ(10, *s2.evaluate());
}

TEST(MapSignal, single)
{
    auto i = signal::input(10);

    /*auto s1 = eraseType(signal::map([](int n)
            {
                return n * 2;
            }, eraseType(i.signal)));*/

    auto s1 = std::move(i.signal) | signal::fmap2([](int n)
            {
                return n * 2;
            });

    EXPECT_FALSE(s1.hasChanged());
    EXPECT_EQ(20, s1.evaluate());

    i.handle.set(20);

    signal::update(s1, {1, us(0)});

    EXPECT_TRUE(s1.hasChanged());
    EXPECT_EQ(40, s1.evaluate());

    signal::update(s1, {2, us(0)});

    EXPECT_FALSE(s1.hasChanged());
    EXPECT_EQ(40, s1.evaluate());

    signal::update(s1, {3, us(0)});

    EXPECT_FALSE(s1.hasChanged());
    EXPECT_EQ(40, s1.evaluate());

    /*
    std::ofstream f("testrun.dot");
    f << s1.annotate().getDot();
    f.close();
    */
}

struct St
{
    template <typename T>
    int operator()(T&&)
    {
        return 10;
    }
};

TEST(apply, apply)
{
    std::tuple<int, std::string>t(10, "test");

    auto f = [](int, std::string)
    {
        return 20;
    };

    btl::apply(f, t);

    btl::apply([](){}, std::tuple<>());

    btl::apply(f,
            std::tuple_cat(std::make_tuple(10),
                std::make_tuple(std::string("jeje"))));

    btl::tuple_map(t, St());
}

TEST(Map, tester)
{
    testSignal([](auto s)
    {
        return signal::map([](int i)
            {
                return i * 2;
            },
            std::move(s)
            );
    });
}

