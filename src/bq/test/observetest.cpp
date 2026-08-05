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

#include <optional>
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
    ctx.setObserveCallback([&] { fired = true; });

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
    ctx.setObserveCallback([&] { ++count; });

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
    ctx.setObserveCallback([&] { fired = true; });

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
    ctx.setObserveCallback([&] { fired = true; });

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
    ctx.setObserveCallback([&] { fired = true; });

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
    ctx.setObserveCallback([&] { ++count; });

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
    ctx.setObserveCallback([&] { ++count; });

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
    ctx.setObserveCallback([&] { fired = true; });

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
    ctx.setObserveCallback([&] { fired = true; });

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
    ctx.setObserveCallback([&] { firedTrue = true; });

    auto data = initialize(ctx, sig);

    pt.handle.push(1);
    EXPECT_TRUE(firedTrue);

    // The false branch was never initialized, so pushing it wakes nothing.
    bool firedFalse = false;
    ctx.setObserveCallback([&] { firedFalse = true; });
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
    ctx.setObserveCallback([&] { fired = true; });

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
    ctx.setObserveCallback([&] { fired = true; });

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
    ctx1.setObserveCallback([&] { fired1 = true; });
    auto data1 = initialize(ctx1, input.signal);

    DataContext ctx2;
    bool fired2 = false;
    ctx2.setObserveCallback([&] { fired2 = true; });
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

// An input reached through two routes registers once (its per-context data is
// deduped by control id at init), so a set() fires the wakeup once.
TEST(SignalContextObserve, diamondInputFiresOnce)
{
    auto input = makeInput<int>(0);
    auto c = makeSignalContext(merge(
                input.signal.map([](int x) { return x; }),
                input.signal.map([](int x) { return x; })));

    int count = 0;
    c.observe([&] { ++count; });

    input.handle.set(1);
    EXPECT_EQ(1, count);
}

// An unshared collect reached through two routes builds one Control per route,
// so an emit fires the wakeup once per route. This is allowed: the wakeup may
// fire more than once, and the frame loop coalesces.
TEST(SignalContextObserve, diamondCollectFiresPerRoute)
{
    auto p = pipe<int>();
    auto col = collect(std::move(p.stream));
    auto c = makeSignalContext(merge(
                col.map([](std::vector<int> const& v) { return v.size(); }),
                col.map([](std::vector<int> const& v) { return v.size(); })));

    int count = 0;
    c.observe([&] { ++count; });

    p.handle.push(1);
    EXPECT_EQ(2, count);
}

// Sharing collapses the diamond back to one Control, so one emit fires once.
TEST(SignalContextObserve, diamondCollectSharedFiresOnce)
{
    auto p = pipe<int>();
    auto col = collect(std::move(p.stream)).share();
    auto c = makeSignalContext(merge(
                col.map([](std::vector<int> const& v) { return v.size(); }),
                col.map([](std::vector<int> const& v) { return v.size(); })));

    int count = 0;
    c.observe([&] { ++count; });

    p.handle.push(1);
    EXPECT_EQ(1, count);
}

// Two distinct changes each fire: the wakeup is not coalesced within a cycle.
TEST(SignalContextObserve, distinctInputsEachFire)
{
    auto a = makeInput<int>(0);
    auto b = makeInput<int>(0);
    auto c = makeSignalContext(a.signal, b.signal);

    int count = 0;
    c.observe([&] { ++count; });

    a.handle.set(1);
    b.handle.set(2);
    EXPECT_EQ(2, count);
}

// ---------------------------------------------------------------------------
// Init/deinit balance under a diamond. The context (and its callback) outlives
// the graph data: after tearing the data down, a set() that still fired would
// mean a registration leaked past its per-context data, so count staying at 0
// proves the unregister was balanced against the register.
// ---------------------------------------------------------------------------

TEST(ObserveDiamond, inputTeardownRebuild)
{
    auto input = makeInput<int>(0);

    DataContext ctx;
    int count = 0;
    ctx.setObserveCallback([&] { ++count; });

    auto sig = merge(input.signal.map([](int x) { return x; }),
                     input.signal.map([](int x) { return x; }));

    {
        auto data = initialize(ctx, sig);
        input.handle.set(1);
        EXPECT_EQ(1, count); // one registration despite two routes
    } // data destroyed once -> ContextDataType destroyed once -> unregister once

    count = 0;
    input.handle.set(2);
    EXPECT_EQ(0, count); // context still alive; nothing fired -> balanced

    {
        auto data = initialize(ctx, sig);
        count = 0;
        input.handle.set(3);
        EXPECT_EQ(1, count); // re-init re-registers cleanly
    }
}

TEST(ObserveDiamond, collectTeardownRebuild)
{
    auto p = pipe<int>();

    DataContext ctx;
    int count = 0;
    ctx.setObserveCallback([&] { ++count; });

    auto col = collect(std::move(p.stream));
    auto sig = merge(col.map([](std::vector<int> const& v) { return v.size(); }),
                     col.map([](std::vector<int> const& v) { return v.size(); }));

    {
        auto data = initialize(ctx, sig);
        p.handle.push(1);
        EXPECT_EQ(2, count); // two Controls
    } // both Controls destroyed -> both fmap subscriptions detach

    count = 0;
    p.handle.push(2);
    EXPECT_EQ(0, count); // no dangling subscription fired

    {
        auto data = initialize(ctx, sig);
        count = 0;
        p.handle.push(3);
        EXPECT_EQ(2, count); // rebuilt cleanly
    }
}

// A shared subgraph (not just a leaf) reached twice: the input inside it inits
// once, and tearing the shared node down releases it once.
TEST(ObserveDiamond, sharedSubgraphTeardown)
{
    auto input = makeInput<int>(0);
    auto shared = input.signal.map([](int x) { return x * 2; }).share();

    DataContext ctx;
    int count = 0;
    ctx.setObserveCallback([&] { ++count; });

    auto sig = merge(shared.map([](int x) { return x; }),
                     shared.map([](int x) { return x; }));

    {
        auto data = initialize(ctx, sig);
        input.handle.set(1);
        EXPECT_EQ(1, count); // shared subgraph -> input inits once
    }

    count = 0;
    input.handle.set(2);
    EXPECT_EQ(0, count); // shared node released -> input released -> unregister
}

// Refcount-driven release: two routes share the input's per-context data.
// Dropping one route must not unregister while the other still holds it; the
// last route out unregisters exactly once (no early or double deinit).
TEST(ObserveDiamond, inputPartialTeardown)
{
    auto input = makeInput<int>(0);

    DataContext ctx;
    int count = 0;
    ctx.setObserveCallback([&] { ++count; });

    auto s1 = input.signal.map([](int x) { return x; });
    auto s2 = input.signal.map([](int x) { return x; });

    auto d1 = initialize(ctx, s1);
    auto d2 = initialize(ctx, s2);
    std::optional<decltype(d1)> route1{ std::move(d1) };
    std::optional<decltype(d2)> route2{ std::move(d2) };

    input.handle.set(1);
    EXPECT_EQ(1, count); // one registration shared by both routes

    route1.reset(); // one route leaves; the other still holds the data
    count = 0;
    input.handle.set(2);
    EXPECT_EQ(1, count); // still registered -> still fires

    route2.reset(); // last route leaves -> data destroyed -> unregister
    count = 0;
    input.handle.set(3);
    EXPECT_EQ(0, count); // unregistered exactly once
}
