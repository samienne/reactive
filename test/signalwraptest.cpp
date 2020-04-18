#include <reactive/signal/signal.h>
#include <reactive/signal/constant.h>

#include <gtest/gtest.h>
#include <string>

using namespace reactive::signal;

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

