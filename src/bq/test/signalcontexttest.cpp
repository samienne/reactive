#include <bq/signal/signal.h>
#include <bq/signal/constant.h>
#include <bq/signal/signalcontext.h>
#include <bq/signal/input.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace bq::signal;

// A SignalContext can hold several differently-typed top-level signals. They
// update in lockstep and each entry is addressed by index.
TEST(signalContext, multipleSignalsUpdateInLockstep)
{
    auto number = makeInput(1);
    auto text = makeInput<std::string>("a");

    auto numberSignal = number.signal.map([](int n) { return n * 10; });

    auto context = makeSignalContext(std::move(numberSignal), text.signal);

    EXPECT_EQ(10, context.evaluate<0>().get<0>());
    EXPECT_EQ("a", context.evaluate<1>().get<0>());

    // Before the first update no entry has changed.
    EXPECT_FALSE(context.didChange<0>());
    EXPECT_FALSE(context.didChange<1>());
    EXPECT_FALSE(context.didAnyChange());

    // Change only the first input, then update the whole context once.
    number.handle.set(5);
    auto result = context.update(FrameInfo(1, {}));

    EXPECT_TRUE(result.didChange);
    EXPECT_TRUE(context.didChange<0>());
    EXPECT_FALSE(context.didChange<1>());
    EXPECT_TRUE(context.didAnyChange());

    EXPECT_EQ(50, context.evaluate<0>().get<0>());
    EXPECT_EQ("a", context.evaluate<1>().get<0>());
}

// The whole point of the shared DataContext: a shared subgraph referenced from
// two entries of one context is updated once per pass, not once per entry.
TEST(signalContext, sharedSubgraphEvaluatedOncePerPass)
{
    auto input = makeInput(1);
    auto counter = std::make_shared<int>(0);

    auto shared = input.signal
        .map([counter](int n)
            {
                ++*counter;
                return n;
            })
        .share();

    // Two entries reference the SAME shared signal.
    auto a = shared.map([](int n) { return n + 1; });
    auto b = shared.map([](int n) { return n + 2; });

    auto context = makeSignalContext(std::move(a), std::move(b));

    // The initial pass (initialize) evaluated the side-effecting map once,
    // despite being reachable from both entries.
    int const afterInit = *counter;

    input.handle.set(7);
    auto result = context.update(FrameInfo(1, {}));

    EXPECT_TRUE(result.didChange);

    // The shared map ran exactly once for the update pass, not once per entry.
    EXPECT_EQ(afterInit + 1, *counter);

    EXPECT_EQ(8, context.evaluate<0>().get<0>());
    EXPECT_EQ(9, context.evaluate<1>().get<0>());
}

// A single-signal context is the one-arg instantiation and is addressed the
// same way: evaluate<0>() returns entry 0's SignalResult, didChange<0>() its
// change flag. There is no non-indexed convenience accessor.
TEST(signalContext, singleSignal)
{
    auto input = makeInput(42);

    auto context = makeSignalContext(input.signal);

    EXPECT_EQ(42, context.evaluate<0>().get<0>());
    EXPECT_FALSE(context.didChange<0>());

    input.handle.set(24);
    auto result = context.update(FrameInfo(1, {}));

    EXPECT_TRUE(result.didChange);
    EXPECT_TRUE(context.didChange<0>());
    EXPECT_EQ(24, context.evaluate<0>().get<0>());
}
