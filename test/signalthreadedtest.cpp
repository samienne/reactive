#include <reactive/signal/input.h>
#include <reactive/signal/count.h>
#include <reactive/signal/constant.h>
#include <reactive/signal.h>

#include <gtest/gtest.h>

#include <iostream>
#include <utility>
#include <string>
#include <cmath>
#include <thread>
#include <chrono>

using namespace reactive;
using us = std::chrono::microseconds;

template <typename TSignal, typename THandle>
void stressSignalThreaded(unsigned int numOfThreads, TSignal sig,
        THandle handle)
{
    auto r = [sig]() mutable
    {
        for (auto i = 0; i < 10000; ++i)
        {
            sig.beginTransaction();
            sig.endTransaction(us(0));
            sig.hasChanged();
            sig.evaluate();
        }
    };

    std::vector<std::thread> threads;
    for (auto i = 0u; i < numOfThreads; ++i)
    {
        threads.emplace_back(r);
    }

    for (auto i = 0u; i < 10000; ++i)
    {
        handle.set(i);
    }

    for (auto& thread : threads)
    {
        thread.join();
    }
}

#if 0
TEST(SignalThreaded, input)
{
    auto i = signal::input(20);

    stressSignalThreaded(10, i.signal, i.handle);
}

TEST(SignalThreaded, typeReduction)
{
    auto i = signal::input(20);

    auto s1 = Signal<int>(i.signal);
    stressSignalThreaded(10, s1, i.handle);
}

TEST(SignalThreaded, lift)
{
    auto i = signal::input(20);

    auto add = [](int a, int b)
    {
        return a + b;
    };

    auto s1 = signal::map(add, i.signal, i.signal);
    stressSignalThreaded(10, s1, i.handle);
}

TEST(SignalThreaded, count)
{
    auto i = signal::input(20);

    stressSignalThreaded(10, signal::count(i.signal), i.handle);
}
#endif

