// TEMP: dedicated stress harness for the whenAll cancel-on-failure race in
// btl/future/whenall.h. Restore/trim before merge — see the PR description.
//
// Scenario (the whenAllCancelOnFail case, hammered): whenAll(f1, f2) where f1
// fails immediately and f2 sleeps, then runs a `.then` that sets a flag. f1's
// failure must cancel f2 — whenAll drops its only strong reference to f2's
// control (values_.reset()) so f2's continuation can never run. The reset is
// handed between init() (calling thread) and reportFailure() (pool thread) via
// a store-buffering handshake on two non-seq_cst atomics; when both loads miss,
// neither resets values_, f2 survives, and its `.then` fires (the flag is set).
//
// One instance is exercised at a time (only f1 and f2 in flight), so neither
// waits behind the other for a pool thread: f1 fails promptly while f2 is still
// sleeping. A correct implementation therefore always cancels f2 with a wide
// timing margin, and an observed set flag is a genuine missed reset — not a
// benign race between two legitimately concurrent completions, nor thread-pool
// starvation delaying f1's failure past f2's sleep.

#include <btl/future/whenall.h>
#include <btl/future/future.h>
#include <btl/async.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace btl::future;
using btl::async;

namespace
{
    // f2 must still be sleeping when f1's failure is processed, so cancellation
    // is unambiguously the correct outcome and has a wide margin to win.
    constexpr auto kSiblingSleep = std::chrono::milliseconds(150);

    // Returns true if f2's continuation leaked (ran despite f1 failing first).
    bool runOnce()
    {
        // Heap-owned so the flag outlives this frame and a late (i.e.
        // not-cancelled) continuation can still be observed setting it.
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

        // Wait out f2's sleep with margin so a surviving continuation has run.
        std::this_thread::sleep_for(kSiblingSleep + std::chrono::milliseconds(20));

        return called->load();
    }
} // namespace

TEST(asyncStress, whenAllCancelOnFail)
{
    constexpr int kIterations = 400;

    int leaked = 0;
    for (int i = 0; i < kIterations; ++i)
    {
        if (runOnce())
            ++leaked;
    }

    // A correct whenAll always cancels the sibling of a failed future before its
    // `.then` runs, so `called` is never observed true.
    EXPECT_EQ(0, leaked)
        << "f2's continuation ran " << leaked << " / " << kIterations
        << " times despite f1 failing first — whenAll failed to cancel it.";
}
