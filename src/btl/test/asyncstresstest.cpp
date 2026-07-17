// TEMP: stress/regression harness for the whenAll cancel-on-failure race in
// btl/future/whenall.h. Restore/trim before merge — see the PR description.
//
// Scenario (the whenAllCancelOnFail case, hammered): whenAll(f1, f2) where f1
// fails immediately and f2 sleeps, then runs a `.then` that sets a flag. f1's
// failure must cancel f2 — whenAll drops its only strong reference to f2's
// control (values_.reset()) so f2's continuation can never run.
//
// The sibling sleep is deliberately generous. A short sleep makes a *correct*
// implementation leak too (a coarse reset-vs-completion timing race under CI
// load), so it does not isolate the handshake defect; a wide margin lets a
// correct reset always win, so an observed continuation is a genuine miss.
//
// NOTE (honesty): the specific store-buffering *double-miss* in the racy code
// (both init() and reportFailure() failing to observe the other's flag, so
// values_ is never reset) is extremely rare — rarer than ~1/400 on the Apple-
// Silicon CI runner — and produces no conflicting memory access, so
// ThreadSanitizer does not flag it. This harness therefore serves mainly as a
// regression guard for the fixed code; it did not, within a practical CI time
// budget, produce a reliably RED signal that discriminates the racy code from
// the fixed code. See the PR description for the full findings.

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
    // Comfortably larger than the time for whenAll to construct and f1 to
    // report its failure, so cancellation always has margin to win.
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

        // Wait out f2's sleep with margin so any surviving continuation has run.
        std::this_thread::sleep_for(kSiblingSleep + std::chrono::milliseconds(30));

        return called->load();
    }
} // namespace

TEST(asyncStress, whenAllCancelOnFail)
{
    constexpr int kIterations = 1000;

    int leaked = 0;
    for (int i = 0; i < kIterations; ++i)
    {
        if (runOnce())
            ++leaked;
    }

    // A correct whenAll cancels the sibling of a failed future before its
    // `.then` runs, so `called` is never observed true.
    EXPECT_EQ(0, leaked)
        << "f2's continuation ran " << leaked << " / " << kIterations
        << " times despite f1 failing first — whenAll failed to cancel it.";
}
