#include <reactive/signal/constant.h>
#include <reactive/signal/map.h>
#include <reactive/signal/mbind.h>
#include <reactive/signal.h>

#include <btl/future/fmap.h>
#include <btl/future/mbind.h>

#include <btl/bundle.h>
#include <btl/fmap.h>
#include <btl/async.h>

#include "gtest/gtest.h"

#include <string>

using namespace reactive;

TEST(Bundle, signalFMap)
{
    auto a = signal::constant(10);
    auto b = signal::constant(20);

    auto c = *btl::bundle(std::move(a), std::move(b))
        .fmap([](int i, int j)
        {
            return i + j;
        });

    AnySignal<int> d = std::move(c);

    EXPECT_EQ(30, d.evaluate());
}

TEST(Bundle, signalMBind)
{
    auto a = signal::constant(10);
    auto b = signal::constant(20);

    auto c = *btl::bundle(std::move(a), std::move(b))
        .mbind([](int i, int j)
        {
            return signal::constant(i + j);
        });

    AnySignal<int> d = std::move(c);

    EXPECT_EQ(30, d.evaluate());
}

TEST(Bundle, futureFMap)
{
    auto a = btl::async([]() { return 10; });
    auto b = btl::async([]() { return 20; });

    auto c = *btl::bundleMove(a, b)
        .fmap([](int i, int j)
        {
            return i + j;
        })
        .fmap([](int n)
        {
            return std::to_string(n);
        })
        ;

    EXPECT_EQ(std::string("30"), std::move(c).get());
}

TEST(Bundle, futureMBind)
{
    auto a = btl::async([]() { return 10; });
    auto b = btl::async([]() { return 20; });

    auto c = *btl::bundleMove(a, b)
        .mbind([](int i, int j)
        {
            return btl::async([=]() { return i + j; });
        })
        .fmap([](int n)
        {
            return std::to_string(n);
        })
        ;

    EXPECT_EQ(std::string("30"), std::move(c).get());
}

