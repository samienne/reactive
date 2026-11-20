#include <ase/dispatcher.h>

#include <gtest/gtest.h>

#include <chrono>
#include <future>

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
        [&]()
        {
            dispatcher.setIdleFunc(Dispatched(),
                period,
                [&]()
                {
                    ++calls;
                    dispatcher.unsetIdleFunc(Dispatched());
                    called.set_value();
                });
        });

    called.get_future().wait();

    std::promise<bool> hasIdleFunc;
    dispatcher.run(
        [&]()
        {
            hasIdleFunc.set_value(dispatcher.hasIdleFunc(Dispatched()));
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
        [&]()
        {
            dispatcher.setIdleFunc(Dispatched(),
                period,
                [&]()
                {
                    ++firstCalls;
                    dispatcher.setIdleFunc(Dispatched(),
                        period,
                        [&]()
                        {
                            ++secondCalls;
                            dispatcher.unsetIdleFunc(Dispatched());
                            secondCalled.set_value();
                        });
                });
        });

    secondCalled.get_future().wait();
    dispatcher.wait();

    EXPECT_EQ(1, firstCalls);
    EXPECT_EQ(1, secondCalls);
}
