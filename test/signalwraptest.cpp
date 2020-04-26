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

class SafeType
{
public:
    struct Data
    {
        bool initialized = true;
    };

    SafeType() : data_(std::make_unique<Data>())
    {
    };

    ~SafeType()
    {
        data_->initialized = false;
        data_.reset();
    }

    SafeType(SafeType const& rhs) :
        data_(std::make_unique<Data>())
    {
        assert(rhs.data_ && rhs.data_->initialized);
        if (!rhs.data_ || !rhs.data_->initialized)
            throw std::runtime_error("unitialized");
    }

    SafeType(SafeType&& rhs) :
        data_(std::make_unique<Data>())
    {
        assert(rhs.data_ && rhs.data_->initialized);

        if (!rhs.data_ || !rhs.data_->initialized)
            throw std::runtime_error("unitialized");

        rhs.data_->initialized = false;
    }


    SafeType& operator=(SafeType const& rhs)
    {
        assert(data_ && data_->initialized);
        assert(rhs.data_ && rhs.data_->initialized);

        if (!data_ || !data_->initialized)
            throw std::runtime_error("unitialized");

        if (!rhs.data_ || !rhs.data_->initialized)
            throw std::runtime_error("unitialized");

        return *this;
    }

    SafeType& operator=(SafeType&& rhs)
    {
        assert(data_ && data_->initialized);
        assert(rhs.data_ && rhs.data_->initialized);

        if (!data_ || !data_->initialized)
            throw std::runtime_error("unitialized");

        if (!rhs.data_ || !rhs.data_->initialized)
            throw std::runtime_error("unitialized");

        data_->initialized = true;
        rhs.data_->initialized = false;

        return *this;
    }


private:
    std::unique_ptr<Data> data_;
};

TEST(Signal, mapReferenceToTemporary)
{
    auto s1 = constant(SafeType())
        .map([](auto const& n) -> SafeType const& { return n; });

    static_assert(std::is_same_v<SafeType const&, SignalType<decltype(s1)>>, "");

    auto s2 = std::move(s1)
        .map([](auto const& n) { return n; });

    static_assert(std::is_same_v<SafeType, SignalType<decltype(s2)>>, "");

    auto s3 = std::move(s2).map([]
            (SafeType const& n) -> decltype(auto)
            {
                return n;
            });

    static_assert(std::is_same_v<SafeType, SignalType<decltype(s3)>>, "");

    auto r = s3.evaluate();

    static_assert(std::is_same_v<SafeType, decltype(r)>, "");

}
