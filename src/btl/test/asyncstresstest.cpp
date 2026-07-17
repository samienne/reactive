// TEMP: dedicated stress harness for the whenAll cancel-on-failure race in
// btl/future/whenall.h. Restore/trim before merge — see the PR description.
//
// Two complementary detectors run over the same whenAllCancelOnFail scenario:
//
//   1. A ThreadSanitizer detector (primary). whenAll(f1, f2, ...) where a
//      sibling fails must drop values_ so the others' continuations never run.
//      In the racy code that reset is performed by either init() (calling
//      thread) or reportFailure() (pool thread), chosen through a two-flag
//      store-buffering handshake. When init() and reportFailure() run truly
//      concurrently they can *both* perform values_.reset(): two threads
//      writing the same std::optional under only acquire/release ordering — a
//      data race TSan reports. To make that concurrency likely, siblings fail
//      immediately (no sleep) while whenAll is still registering callbacks, and
//      many instances are constructed back-to-back. The fixed code serialises
//      every values_ access under a spinlock, so TSan stays silent.
//
//   2. A functional detector (secondary, wide margin). A failing sibling must
//      cancel a *sleeping* sibling before its `.then` runs. The sleep is made
//      generous so a correct implementation always wins the cancellation with
//      margin; an observed continuation means the reset was skipped. (A short
//      sleep instead measures a coarser reset-vs-completion timing race that
//      does not isolate the handshake, so a comfortable margin is used here.)

#include <btl/future/whenall.h>
#include <btl/future/future.h>
#include <btl/async.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace btl::future;
using btl::async;

namespace
{
    // TSan detector: maximise init()/reportFailure() concurrency so the racy
    // two-flag handshake can drive both threads to reset values_ at once.
    TEST(asyncStress, whenAllResetDataRace)
    {
        constexpr int kIterations = 20000;

        for (int i = 0; i < kIterations; ++i)
        {
            // Several siblings that fail immediately, so a failure is reported
            // from a pool thread while whenAll's init() is still iterating.
            auto f1 = async([]() -> int
                    {
                        throw std::runtime_error("boom");
                        return 1;
                    });
            auto f2 = async([]() -> int
                    {
                        throw std::runtime_error("boom");
                        return 2;
                    });
            auto f3 = async([]() -> int { return 3; });

            auto r = whenAll(std::move(f1), std::move(f2), std::move(f3));

            try
            {
                std::move(r).get();
            }
            catch (std::runtime_error const&)
            {
            }
        }
    }

    // f2 must still be sleeping when the failure is processed, so cancellation
    // is unambiguously correct and wins with margin.
    constexpr auto kSiblingSleep = std::chrono::milliseconds(150);

    bool runOnce()
    {
        auto called = std::make_shared<std::atomic_bool>(false);

        auto f1 = async([]() -> int
                {
                    throw std::runtime_error("test error");
                    return 10;
                });

        auto f2 = async([]()
                {
                    std::this_thread::sleep_for(kSiblingSleep);
                })
                .then([called]()
                {
                    called->store(true);
                });

        auto r = whenAll(std::move(f1), std::move(f2));

        bool threw = false;
        try
        {
            std::move(r).get();
        }
        catch (std::runtime_error const&)
        {
            threw = true;
        }
        EXPECT_TRUE(threw);

        std::this_thread::sleep_for(kSiblingSleep + std::chrono::milliseconds(30));

        return called->load();
    }
} // namespace

TEST(asyncStress, whenAllCancelOnFail)
{
    constexpr int kIterations = 300;

    int leaked = 0;
    for (int i = 0; i < kIterations; ++i)
    {
        if (runOnce())
            ++leaked;
    }

    EXPECT_EQ(0, leaked)
        << "f2's continuation ran " << leaked << " / " << kIterations
        << " times despite f1 failing first — whenAll failed to cancel it.";
}
