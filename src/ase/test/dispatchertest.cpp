#include <ase/dispatcher.h>

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <vector>

using namespace ase;

namespace
{
    std::chrono::milliseconds const period(1);
} // anonymous namespace

TEST(dispatcher, idleFuncThatUnsetsItselfRunsOnce)
{
    Dispatcher dispatcher;
    int calls = 0;
    std::promise<void> called;

    dispatcher.run(
        [&](Dispatched d)
        {
            dispatcher.setIdleFunc(d,
                period,
                [&](Dispatched d)
                {
                    ++calls;
                    dispatcher.unsetIdleFunc(d);
                    called.set_value();
                });
        });

    called.get_future().wait();

    std::promise<bool> hasIdleFunc;
    dispatcher.run(
        [&](Dispatched d)
        {
            hasIdleFunc.set_value(dispatcher.hasIdleFunc(d));
        });

    EXPECT_FALSE(hasIdleFunc.get_future().get());
    dispatcher.wait();
    EXPECT_EQ(1, calls);
}

TEST(dispatcher, idleFuncThatReplacesItselfSwitches)
{
    Dispatcher dispatcher;
    int firstCalls = 0;
    int secondCalls = 0;
    std::promise<void> secondCalled;

    dispatcher.run(
        [&](Dispatched d)
        {
            dispatcher.setIdleFunc(d,
                period,
                [&](Dispatched d)
                {
                    dispatcher.setIdleFunc(d,
                        period,
                        [&](Dispatched d)
                        {
                            ++secondCalls;
                            dispatcher.unsetIdleFunc(d);
                            secondCalled.set_value();
                        });
                    ++firstCalls;
                });
        });

    secondCalled.get_future().wait();
    dispatcher.wait();

    EXPECT_EQ(1, firstCalls);
    EXPECT_EQ(1, secondCalls);
}

TEST(dispatcher, statefulIdleFuncKeepsItsState)
{
    Dispatcher dispatcher;
    std::vector<int> seen;
    std::promise<void> done;

    dispatcher.run(
        [&](Dispatched d)
        {
            dispatcher.setIdleFunc(d,
                period,
                [&, calls = 0](Dispatched d) mutable
                {
                    seen.push_back(++calls);
                    if (seen.size() == 3)
                    {
                        dispatcher.unsetIdleFunc(d);
                        done.set_value();
                    }
                });
        });

    done.get_future().wait();
    dispatcher.wait();

    EXPECT_EQ((std::vector<int>{ 1, 2, 3 }), seen);
}
