#include <reactive/signal/split.h>
#include <reactive/signal/waitfor.h>
#include <reactive/signal/constant.h>
#include <reactive/signal/update.h>

#include <btl/delayed.h>
#include <btl/always.h>

#include "signaltester.h"
#include <gtest/gtest.h>

#include <iostream>

using namespace reactive;

TEST(signal, waitFor)
{
    auto f = btl::delayed(std::chrono::seconds(1), btl::always(1));
    auto s = reactive::signal::waitFor(10, std::move(f));

    EXPECT_EQ(10, s.evaluate());

    std::this_thread::sleep_for(std::chrono::seconds(2));

    signal::update(s, reactive::signal::FrameInfo(1u, std::chrono::seconds(2)));

    EXPECT_TRUE(s.hasChanged());
    EXPECT_EQ(1, s.evaluate());
}

TEST(signal, split)
{
    auto s = signal::constant(std::make_tuple(10, std::string("20")));

    auto tuple = signal::split(std::move(s));

    auto s1 = std::get<0>(std::move(tuple));
    auto s2 = std::get<1>(std::move(tuple));

    EXPECT_EQ(10, s1.evaluate());
    EXPECT_EQ(std::string("20"), s2.evaluate());

    Signal<int> s3 = std::move(s1);
    Signal<std::string> s4 = std::move(s2);

    EXPECT_EQ(10, s3.evaluate());
    EXPECT_EQ(std::string("20"), s4.evaluate());
}

TEST(signal, cacheOptimizations)
{
    auto make = []()
    {
        return signal::cache(signal::constant(std::string("test")));
    };

    auto s1 = signal::cache(make());
    static_assert(std::is_same<
            decltype(s1),
            signal::Cache<signal::Constant<std::string>>
            >::value, "");

    auto s2 = signal::share(make());

    static_assert(std::is_same<
            decltype(s2),
            signal::Share<signal::Cache<signal::Constant<std::string>>>
            >::value, "");
}

TEST(signal, shareOptimizations)
{
    auto make = []()
    {
        return signal::share(signal::constant(std::string("test")));
    };

    auto s = make();

    static_assert(std::is_same<
            decltype(s),
            signal::Share<signal::Constant<std::string>>
            >::value, "");

    auto s1 = signal::cache(make());

    static_assert(std::is_same<
            decltype(s1),
            signal::Share<signal::Constant<std::string>>
            >::value, "");

    auto s2 = signal::share(make());
    static_assert(std::is_same<
            decltype(s2),
            signal::Share<signal::Constant<std::string>>
            >::value, "");
}

TEST(signal, tester)
{
    testSignal([](auto s)
    {
        return signal::cache(std::move(s));
    });

    testSignal([](auto s)
    {
        return signal::share(std::move(s));
    });
}

