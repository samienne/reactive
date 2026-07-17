// TEMP: dedicated stress harness for the whenAll cancel-on-failure
// store-buffering race in btl/future/whenall.h (ARM64-only under normal
// timing; deterministic here under ThreadSanitizer). Restore/trim before
// merge — see the PR description.
//
// The functional signal is: whenAll(f1, f2) where f1 fails immediately must
// cancel f2 before f2's `.then` runs. If the SB handshake double-misses,
// values_ is never reset, f2 survives, its `.then` fires, and `called`
// becomes true. Under TSan the un-serialized access to values_ (init() vs
// reportFailure() on two threads) is reported directly.

#include <btl/future/whenall.h>
#include <btl/future/future.h>
#include <btl/async.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

using namespace btl::future;
using btl::async;

namespace
{
    // One iteration of the whenAllCancelOnFail scenario. f1 fails immediately;
    // f2 sleeps briefly and then runs a `.then`. Because f1 fails first,
    // whenAll must drop its reference to f2 (values_.reset()) so f2's `.then`
    // never runs. The sleep is what makes cancellation the *correct* outcome:
    // f1's failure has time to reach whenAll before f2 finishes, so a `called`
    // of true is a genuine defect (the reset was missed), not a benign race
    // between two legitimately-concurrent completions.
    //
    // The reset is handed between init() (calling thread) and reportFailure()
    // (pool thread) via a store-buffering handshake on two non-seq_cst
    // atomics; when both loads miss, neither resets values_. On x86 the
    // lock-prefixed CAS drains the store buffer so the miss is not observed;
    // on ARM64 it is. Either way, ThreadSanitizer reports the un-serialized
    // access to values_ deterministically.
    bool runOnce()
    {
        auto f1 = async([]() -> int
                {
                    throw std::runtime_error("test error");
                    return 10;
                });

        std::atomic_bool called = false;

        auto f2 = async([]()
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                })
                .then([&called]()
                {
                    called.store(true);
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

        // Give any surviving (i.e. not-cancelled) f2 continuation time to run
        // so a functional miss is observable even without TSan.
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        EXPECT_TRUE(threw);
        return called.load();
    }
} // namespace

TEST(asyncStress, whenAllCancelOnFail)
{
    constexpr int kIterations = 3000;

    int leaked = 0;
    for (int i = 0; i < kIterations; ++i)
    {
        if (runOnce())
            ++leaked;
    }

    // Under a correct implementation the failed future always cancels its
    // sibling before the `.then` runs, so `called` is never observed true.
    EXPECT_EQ(0, leaked)
        << "f2's continuation ran " << leaked << " / " << kIterations
        << " times despite f1 failing first — whenAll failed to cancel it.";
}
