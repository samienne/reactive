#include <bq/stream/collect.h>
#include <bq/stream/pipe.h>

#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>
#include <bq/signal/datacontext.h>
#include <bq/signal/constant.h>
#include <bq/signal/conditional.h>
#include <bq/signal/merge.h>
#include <bq/signal/combine.h>
#include <bq/signal/input.h>
#include <bq/signal/arraysignal.h>

#include <gtest/gtest.h>

#include <vector>

using namespace bq;
using namespace bq::signal;
using namespace bq::stream;

// Design B: the wakeup lives on the DataContext, not on the signals. A caller
// arms the context with observe(); external leaves (input, collect) bind a weak
// reference to that context's control when their per-context data is created at
// init, and fire it on set()/emit without evaluating the graph. There is no
// per-signal observe() traversal.
//
// The control is always armed: it stays armed for the context's lifetime and
// fires on every external change (coalescing is left to the frame loop). A
// leaf's registration is dropped when its per-context data is destroyed, so a
// branch swapped out of a conditional or join stops waking the context.
//
// These tests arm a context, initialize a graph into it (which wires the
// leaves), then drive a leaf and assert the callback fired.

namespace
{
    // Initializes a signal impl into a context and returns its DataType, with
    // the frame data swapped once as a live context does after construction.
    // Initialization is what wires the context's leaves to its wakeup.
    template <typename TSignal>
    auto initialize(DataContext& ctx, TSignal& sig)
    {
        auto data = sig.unwrap().initialize(ctx, FrameInfo(0, {}));
        ctx.swapFrameData();
        return data;
    }
} // namespace

// A plain value input is an external leaf: arming the context and then set()ting
// a new value wakes it.
TEST(Observe, inputFiresOnSet)
{
    auto input = makeInput<int>(5);

    DataContext ctx;
    bool fired = false;
    ctx.observe([&] { fired = true; });

    auto data = initialize(ctx, input.signal);

    EXPECT_FALSE(fired);
    input.handle.set(10);
    EXPECT_TRUE(fired);
}

// Always armed: every set() fires the wakeup, not just the first.
TEST(Observe, firesOnEachSet)
{
    auto input = makeInput<int>(0);

    DataContext ctx;
    int count = 0;
    ctx.observe([&] { ++count; });

    auto data = initialize(ctx, input.signal);

    input.handle.set(1);
    EXPECT_EQ(1, count);

    input.handle.set(2);
    EXPECT_EQ(2, count);
}

// The wake does not evaluate the graph: the new value is not visible through
// evaluate() until an update() swaps it into the current value.
TEST(Observe, wakeDoesNotEvaluate)
{
    auto input = makeInput<int>(5);

    DataContext ctx;
    bool fired = false;
    ctx.observe([&] { fired = true; });

    auto data = initialize(ctx, input.signal);

    input.handle.set(10);
    EXPECT_TRUE(fired);

    // The wake fired, but evaluate() still sees the old value until update().
    EXPECT_EQ(5, input.signal.unwrap().evaluate(ctx, data).get<0>());
    input.signal.unwrap().update(ctx, data, FrameInfo(1, {}));
    EXPECT_EQ(10, input.signal.unwrap().evaluate(ctx, data).get<0>());
}

// A collect leaf fires the context wakeup when its stream emits.
TEST(Observe, collectFiresOnEmit)
{
    auto p = pipe<int>();
    auto sig = collect(std::move(p.stream));

    DataContext ctx;
    bool fired = false;
    ctx.observe([&] { fired = true; });

    auto data = initialize(ctx, sig);

    EXPECT_FALSE(fired);
    p.handle.push(42);
    EXPECT_TRUE(fired);
}

// A mapped node wires the same leaf: a push at the collect wakes the context.
TEST(Observe, throughMap)
{
    auto p = pipe<int>();
    auto sig = collect(std::move(p.stream)).map(
            [](std::vector<int> const& v) { return v.size(); });

    DataContext ctx;
    bool fired = false;
    ctx.observe([&] { fired = true; });

    auto data = initialize(ctx, sig);

    p.handle.push(7);
    EXPECT_TRUE(fired);
}

// merge wires all of its children: a change in either leaf wakes the context.
TEST(Observe, throughMerge)
{
    auto pa = pipe<int>();
    auto pb = pipe<int>();
    auto sig = merge(collect(std::move(pa.stream)), collect(std::move(pb.stream)));

    DataContext ctx;
    int count = 0;
    ctx.observe([&] { ++count; });

    auto data = initialize(ctx, sig);

    pa.handle.push(1);
    EXPECT_EQ(1, count);

    pb.handle.push(2);
    EXPECT_EQ(2, count);
}

// combine wires every child leaf.
TEST(Observe, throughCombine)
{
    auto pa = pipe<int>();
    auto pb = pipe<int>();

    std::vector<AnySignal<std::vector<int>>> children;
    children.push_back(collect(std::move(pa.stream)).eraseType());
    children.push_back(collect(std::move(pb.stream)).eraseType());
    auto sig = combine(std::move(children));

    DataContext ctx;
    int count = 0;
    ctx.observe([&] { ++count; });

    auto data = initialize(ctx, sig);

    pa.handle.push(1);
    EXPECT_EQ(1, count);

    pb.handle.push(2);
    EXPECT_EQ(2, count);
}

// join wires the leaf reachable through the current inner signal.
TEST(Observe, throughJoin)
{
    auto p = pipe<int>();
    auto inner = collect(std::move(p.stream));
    auto sig = constant(inner).join();

    DataContext ctx;
    bool fired = false;
    ctx.observe([&] { fired = true; });

    auto data = initialize(ctx, sig);

    p.handle.push(42);
    EXPECT_TRUE(fired);
}

// join(ArraySignal) wires each element present at init.
TEST(Observe, throughArrayJoin)
{
    auto p = pipe<int>();
    auto element = collect(std::move(p.stream)).eraseType();

    ArraySignal<AnySignal<std::vector<int>>> array(std::move(element));
    auto sig = join(array);

    DataContext ctx;
    bool fired = false;
    ctx.observe([&] { fired = true; });

    auto data = initialize(ctx, sig);

    p.handle.push(42);
    EXPECT_TRUE(fired);
}

// A conditional wires only the currently active branch; the inactive branch is
// never initialized, so its leaf is not wired.
TEST(Observe, conditionalActiveBranchOnly)
{
    auto pt = pipe<int>();
    auto pf = pipe<int>();
    auto cond = makeInput<bool>(true);

    auto sig = conditional(cond.signal,
            collect(std::move(pt.stream)),
            collect(std::move(pf.stream)));

    DataContext ctx;
    bool firedTrue = false;
    ctx.observe([&] { firedTrue = true; });

    auto data = initialize(ctx, sig);

    pt.handle.push(1);
    EXPECT_TRUE(firedTrue);

    // The false branch was never initialized, so pushing it wakes nothing.
    bool firedFalse = false;
    ctx.observe([&] { firedFalse = true; });
    pf.handle.push(2);
    EXPECT_FALSE(firedFalse);
}

// Flipping the condition destroys the old branch's data (unregistering its leaf)
// and initializes the new one (wiring its leaf), so the wakeup follows the swap.
TEST(Observe, conditionalRewiresOnFlip)
{
    auto pt = pipe<int>();
    auto pf = pipe<int>();
    auto cond = makeInput<bool>(true);

    auto sig = conditional(cond.signal,
            collect(std::move(pt.stream)),
            collect(std::move(pf.stream)));

    DataContext ctx;
    bool fired = false;
    ctx.observe([&] { fired = true; });

    auto data = initialize(ctx, sig);

    pt.handle.push(1);
    EXPECT_TRUE(fired);

    // Switch to the false branch and update so the conditional swaps its data.
    cond.handle.set(false);
    sig.unwrap().update(ctx, data, FrameInfo(1, {}));

    fired = false;

    // The true branch's data was destroyed, so pushing it no longer fires.
    pt.handle.push(2);
    EXPECT_FALSE(fired);

    // The false branch is now wired.
    pf.handle.push(3);
    EXPECT_TRUE(fired);
}

// A leaf's registration is tied to its per-context data: once that data is
// destroyed, the leaf no longer wakes the context.
TEST(Observe, cleanupOnDataDestruction)
{
    auto input = makeInput<int>(0);

    DataContext ctx;
    bool fired = false;
    ctx.observe([&] { fired = true; });

    {
        auto data = initialize(ctx, input.signal);
    }

    input.handle.set(1);
    EXPECT_FALSE(fired);
}

// A single input feeds two contexts; each registers its own wakeup, so one
// set() fires both.
TEST(Observe, multipleContextsFire)
{
    auto input = makeInput<int>(0);

    DataContext ctx1;
    bool fired1 = false;
    ctx1.observe([&] { fired1 = true; });
    auto data1 = initialize(ctx1, input.signal);

    DataContext ctx2;
    bool fired2 = false;
    ctx2.observe([&] { fired2 = true; });
    auto data2 = initialize(ctx2, input.signal);

    input.handle.set(1);
    EXPECT_TRUE(fired1);
    EXPECT_TRUE(fired2);
}

// ---------------------------------------------------------------------------
// SignalContext::observe -- the public entry point. It arms the context's
// wakeup, which the graph's leaves are already wired to.
// ---------------------------------------------------------------------------

// Arming a context over an input leaf wakes on set(), and the wake does not
// evaluate: the value is pending until update() applies it.
TEST(SignalContextObserve, firesOnSet)
{
    auto input = makeInput<int>(5);
    auto c = makeSignalContext(input.signal);

    bool fired = false;
    c.observe([&] { fired = true; });

    EXPECT_FALSE(fired);
    input.handle.set(10);
    EXPECT_TRUE(fired);

    // The wake did not evaluate: the value is pending until update().
    EXPECT_EQ(5, c.evaluate<0>().get<0>());
    c.update(FrameInfo(1, {}));
    EXPECT_EQ(10, c.evaluate<0>().get<0>());
}

// The wakeup reaches leaves through combinators: a mapped input still wakes the
// context on set().
TEST(SignalContextObserve, throughMap)
{
    auto input = makeInput<int>(1);
    auto c = makeSignalContext(input.signal.map([](int n) { return n * 2; }));

    bool fired = false;
    c.observe([&] { fired = true; });

    input.handle.set(4);
    EXPECT_TRUE(fired);
}

// The wakeup covers every signal the context holds, so a change in either of two
// independent leaves wakes it.
TEST(SignalContextObserve, acrossAllSignals)
{
    auto pa = pipe<int>();
    auto pb = pipe<int>();
    auto c = makeSignalContext(
            collect(std::move(pa.stream)),
            collect(std::move(pb.stream)));

    int count = 0;
    c.observe([&] { ++count; });

    pa.handle.push(1);
    EXPECT_EQ(1, count);

    pb.handle.push(2);
    EXPECT_EQ(2, count);
}

// Always armed: each external change fires, not just the first.
TEST(SignalContextObserve, firesOnEachChange)
{
    auto input = makeInput<int>(0);
    auto c = makeSignalContext(input.signal);

    int count = 0;
    c.observe([&] { ++count; });

    input.handle.set(1);
    EXPECT_EQ(1, count);

    input.handle.set(2);
    EXPECT_EQ(2, count);
}
