#include <reactive/signal2/signal.h>
#include <reactive/signal2/constant.h>
#include <reactive/signal2/signalcontext.h>
#include <reactive/signal2/input.h>

#include <gtest/gtest.h>

using namespace reactive::signal2;

TEST(Signal2, constant)
{
    auto s = constant(10);
    AnySignal<int> ss = s;

    auto data = s.initialize();

    EXPECT_EQ(10, s.evaluate(data).get<0>());

    static_assert(std::is_same_v<int const&, decltype(s.evaluate(data).get<0>())>);

    FrameInfo frame(1, signal_time_t(10));

    auto r = s.update(data, frame);
    EXPECT_EQ(r.nextUpdate, std::nullopt);
}

TEST(Signal2, signalContext)
{
    auto c = makeSignalContext(constant(42));

    UpdateResult r = c.update(FrameInfo(1, signal_time_t(0)));

    EXPECT_EQ(std::nullopt, r.nextUpdate);

    static_assert(std::is_same_v<int const&, decltype(c.evaluate())>);

    int v = c.evaluate();

    EXPECT_EQ(42, v);
}

TEST(Signal2, signalInput)
{
    auto input = makeInput(42);

    auto c = makeSignalContext(input.signal);

    static_assert(std::is_same_v<SignalContext<int const&>, decltype(c)>);

    EXPECT_EQ(42, c.evaluate());

    input.handle.set(22);

    EXPECT_EQ(42, c.evaluate());

    auto r = c.update(FrameInfo(1, {}));
    EXPECT_EQ(std::nullopt, r.nextUpdate);

    EXPECT_EQ(22, c.evaluate());

    static_assert(std::is_same_v<int const&, decltype(c.evaluate())>);
}

TEST(Signal2, map)
{
    auto input = makeInput(42);

    auto s = input.signal.map([](int n)
                {
                    return n * 2;
                });

    auto c = makeSignalContext(std::move(s));

    EXPECT_EQ(84, c.evaluate());

    auto r = c.update(FrameInfo(1, {}));

    EXPECT_FALSE(r.didChange);
    EXPECT_EQ(std::nullopt, r.nextUpdate);

    input.handle.set(12);

    EXPECT_EQ(84, c.evaluate());

    r = c.update(FrameInfo(2, {}));

    EXPECT_EQ(24, c.evaluate());
    EXPECT_TRUE(r.didChange);
    EXPECT_EQ(std::nullopt, r.nextUpdate);
}
