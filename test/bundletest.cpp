#include <reactive/signal/constant.h>
#include <reactive/signal/map.h>
#include <reactive/signal/mbind.h>
#include <reactive/signal/signal.h>

#include <btl/future/whenall.h>

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

