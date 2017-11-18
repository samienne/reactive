#include <reactive/signal/map.h>
#include <reactive/signal/input.h>
#include <reactive/signal/constant.h>
#include <reactive/signal.h>

#include <gtest/gtest.h>

#include <iostream>
#include <string>
#include <cmath>

using namespace reactive;

bool near(float a, float b)
{
    return std::abs(a-b) < 0.0001f;
}

class Functional : public ::testing::Test
{
public:
    Functional()
    {
        auto add = [](int a, int b)
        {
            return a + b;
        };

        std::cout << "ConstSignal<int> "
            << sizeof(signal::Constant<int>) << std::endl;
        std::cout << "InputSignal<int> " << sizeof(signal::InputSignal<int>)
            << std::endl;

        std::cout << "LiftedSignal<int> " << sizeof(signal::map(add,
                    signal::constant(1), signal::constant(2)))
            << std::endl;
    }
};

TEST(Functional, constant)
{
    auto c = signal::constant(10.0f);

    EXPECT_EQ(10.0f, c.evaluate());

    EXPECT_FALSE(c.hasChanged());
}

