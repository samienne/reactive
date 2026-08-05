#include <bq/signal/signal.h>
#include <bq/signal/input.h>
#include <bq/signal/combine.h>
#include <bq/signal/signalcontext.h>

#include <gtest/gtest.h>

#include <vector>

using namespace bq::signal;

namespace
{
    // Copy/move tally shared by every instance descending from a single seed.
    // The seed hands this pointer to each copy/move so the whole family reports
    // to one set of counters.
    struct Counters
    {
        int copyCtor = 0;
        int copyAssign = 0;
        int moveCtor = 0;
        int moveAssign = 0;

        // Copies/assigns that carried a non-empty payload -- the ones that must
        // stay at zero once the accumulator is move-threaded.
        int heavyCopyCtor = 0;
        int heavyCopyAssign = 0;

        int foldCalls = 0;
    };

    // A movable, copyable stand-in for a heavy fold-state (e.g. the planned
    // Cassowary solver). `apply` is an incremental delta-apply that mutates
    // internal state, proving the fold-state persists and accumulates.
    struct CountingSolver
    {
        std::vector<int> tableau;
        Counters* counters = nullptr;

        CountingSolver() = default; // empty seed is cheap
        explicit CountingSolver(Counters* c) : counters(c) {}

        CountingSolver(CountingSolver const& o)
            : tableau(o.tableau), counters(o.counters)
        {
            if (counters)
            {
                ++counters->copyCtor;
                if (!o.tableau.empty())
                    ++counters->heavyCopyCtor;
            }
        }

        CountingSolver(CountingSolver&& o) noexcept
            : tableau(std::move(o.tableau)), counters(o.counters)
        {
            if (counters)
                ++counters->moveCtor;
        }

        CountingSolver& operator=(CountingSolver const& o)
        {
            tableau = o.tableau;
            counters = o.counters;
            if (counters)
            {
                ++counters->copyAssign;
                if (!o.tableau.empty())
                    ++counters->heavyCopyAssign;
            }
            return *this;
        }

        CountingSolver& operator=(CountingSolver&& o) noexcept
        {
            tableau = std::move(o.tableau);
            counters = o.counters;
            if (counters)
                ++counters->moveAssign;
            return *this;
        }

        void apply(std::vector<int> const& delta)
        {
            for (int v : delta)
                tableau.push_back(v);
        }

        int size() const { return static_cast<int>(tableau.size()); }
    };
}

// (a) + (b): across several changed rounds the heavy payload is only moved,
// never copied, and the fold-state accumulates incrementally (each delta-apply
// sees the prior state). The terminal is a small mapped result read via const&,
// so the only value flowing through the fold is the solver, threaded by move.
TEST(withPrevious, movesAccumulatorNeverCopiesHeavyPayload)
{
    Counters counters;

    auto input = makeInput<std::vector<int>>(std::vector<int>{ 1 });

    auto solution = input.signal.withPrevious(
            [&counters](CountingSolver solver, std::vector<int> delta)
            {
                ++counters.foldCalls;
                solver.apply(delta);      // incremental delta-apply
                return solver;            // same instance, moved out
            },
            CountingSolver(&counters))    // empty seed carrying the tally
        .map([](CountingSolver const& s)  // read-only const& -> small result
            {
                return s.size();
            });

    auto c = makeSignalContext(solution);

    // Initialization runs the fold once against the initial input.
    EXPECT_EQ(1, c.evaluate<0>().get<0>());
    EXPECT_EQ(1, counters.foldCalls);

    for (int frame = 1; frame <= 5; ++frame)
    {
        input.handle.set(std::vector<int>{ frame * 10 });
        auto r = c.update(FrameInfo(frame, {}));
        EXPECT_TRUE(r.didChange);
    }

    // (b) fold-state persisted and accumulated: seed round + 5 rounds.
    EXPECT_EQ(6, c.evaluate<0>().get<0>());
    EXPECT_EQ(6, counters.foldCalls);

    // (a) the heavy payload was never copied, only moved.
    EXPECT_EQ(0, counters.heavyCopyCtor);
    EXPECT_EQ(0, counters.heavyCopyAssign);

    // The only copy allowed is the empty seed copied once into the per-context
    // fold-state; no populated solver is ever copied.
    EXPECT_LE(counters.copyCtor, 1);
    EXPECT_EQ(0, counters.copyAssign);

    // Sanity: the accumulator really was threaded by move.
    EXPECT_GT(counters.moveCtor + counters.moveAssign, 0);
}

// (c): with .map(read const&).share() and two downstream readers combined in a
// single context, the fold evaluates exactly once per frame (not forked), and
// the accumulator is still never heavy-copied.
TEST(withPrevious, sharedFanOutSingleEvalNoCopies)
{
    Counters counters;

    auto input = makeInput<std::vector<int>>(std::vector<int>{ 1 });

    auto shared = input.signal.withPrevious(
            [&counters](CountingSolver solver, std::vector<int> delta)
            {
                ++counters.foldCalls;
                solver.apply(delta);
                return solver;
            },
            CountingSolver(&counters))
        .map([](CountingSolver const& s)     // read-only const&, small result
            {
                return s.size();
            })
        .share();                            // share the small results

    // Two downstream readers of the shared node inside one context.
    std::vector<AnySignal<int>> readers;
    readers.push_back(shared.map([](int n) { return n; }));
    readers.push_back(shared.map([](int n) { return n * 2; }));
    auto combined = combine(readers);

    auto c = makeSignalContext(combined);

    // Initialize: the fold runs exactly once even though two readers exist.
    auto v = c.evaluate<0>().get<0>();
    EXPECT_EQ(1, v.at(0));
    EXPECT_EQ(2, v.at(1));
    EXPECT_EQ(1, counters.foldCalls);

    for (int frame = 1; frame <= 4; ++frame)
    {
        input.handle.set(std::vector<int>{ frame });
        c.update(FrameInfo(frame, {}));
    }

    // Exactly one fold evaluation per frame despite the fan-out: init + 4.
    EXPECT_EQ(5, counters.foldCalls);

    // Accumulated fold-state visible through the shared/mapped result.
    v = c.evaluate<0>().get<0>();
    EXPECT_EQ(5, v.at(0));
    EXPECT_EQ(10, v.at(1));

    // The solver was never copied: map reads via const&, share caches only the
    // small int result.
    EXPECT_EQ(0, counters.heavyCopyCtor);
    EXPECT_EQ(0, counters.heavyCopyAssign);
    EXPECT_EQ(0, counters.copyAssign);
    EXPECT_LE(counters.copyCtor, 1);
}
