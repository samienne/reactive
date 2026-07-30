#include <bq/stream/collect.h>
#include <bq/stream/pipe.h>

#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>
#include <bq/signal/datacontext.h>
#include <bq/signal/constant.h>
#include <bq/signal/merge.h>
#include <bq/signal/combine.h>
#include <bq/signal/input.h>
#include <bq/signal/arraysignal.h>

#include <gtest/gtest.h>

#include <vector>

using namespace bq;
using namespace bq::signal;
using namespace bq::stream;

// observe() registers a callback on every leaf that an external source can wake,
// so that the wake fires immediately without evaluating the graph. Nothing in
// the toolkit drives observe today; these tests pin the mechanism down by
// driving the signal impls directly, exactly as SignalContext does internally.

namespace
{
    // Initializes a signal impl into a context and returns its DataType, with
    // the frame data swapped once as a live context does after construction.
    // The signal is passed by lvalue because observe() needs the non-const
    // unwrap().
    template <typename TSignal>
    auto initialize(DataContext& ctx, TSignal& sig)
    {
        auto data = sig.unwrap().initialize(ctx, FrameInfo(0, {}));
        ctx.swapFrameData();
        return data;
    }
} // namespace

// A collect leaf fires its observer exactly once, synchronously, when the
// stream emits.
TEST(Observe, collectLeafFires)
{
    auto p = pipe<int>();
    auto sig = collect(std::move(p.stream));

    DataContext ctx;
    auto data = initialize(ctx, sig);

    bool fired = false;
    auto conn = sig.unwrap().observe(ctx, data, [&] { fired = true; });

    EXPECT_FALSE(fired);
    p.handle.push(42);
    EXPECT_TRUE(fired);
}

// The wake does not evaluate the graph: the pushed value is not visible through
// evaluate() until an update() swaps it into the current values.
TEST(Observe, wakeDoesNotEvaluate)
{
    auto p = pipe<int>();
    auto sig = collect(std::move(p.stream));

    DataContext ctx;
    auto data = initialize(ctx, sig);

    bool fired = false;
    auto conn = sig.unwrap().observe(ctx, data, [&] { fired = true; });

    p.handle.push(42);
    EXPECT_TRUE(fired);

    // The observer fired, but the value is still only pending: evaluate() sees
    // nothing until update() moves it across.
    EXPECT_TRUE(sig.unwrap().evaluate(ctx, data).get<0>().empty());

    sig.unwrap().update(ctx, data, FrameInfo(1, {}));
    EXPECT_EQ((std::vector<int>{ 42 }), sig.unwrap().evaluate(ctx, data).get<0>());
}

// observe is one-shot: firing consumes the callback, so a second emission wakes
// nothing until the observer is re-armed by observing again.
TEST(Observe, collectIsOneShot)
{
    auto p = pipe<int>();
    auto sig = collect(std::move(p.stream));

    DataContext ctx;
    auto data = initialize(ctx, sig);

    int count = 0;
    auto c1 = sig.unwrap().observe(ctx, data, [&] { ++count; });

    p.handle.push(1);
    EXPECT_EQ(1, count);

    // Not re-armed: the callback list was cleared as it fired.
    p.handle.push(2);
    EXPECT_EQ(1, count);

    // Re-arming makes it fire again.
    auto c2 = sig.unwrap().observe(ctx, data, [&] { ++count; });
    p.handle.push(3);
    EXPECT_EQ(2, count);
}

// Disconnecting the returned connection unregisters the callback before it can
// fire.
TEST(Observe, disconnectStopsFiring)
{
    auto p = pipe<int>();
    auto sig = collect(std::move(p.stream));

    DataContext ctx;
    auto data = initialize(ctx, sig);

    bool fired = false;
    auto conn = sig.unwrap().observe(ctx, data, [&] { fired = true; });
    conn.disconnect();

    p.handle.push(42);
    EXPECT_FALSE(fired);
}

// map forwards observe to its single upstream, so an external change at the leaf
// still wakes the observer through the mapped node.
TEST(Observe, throughMap)
{
    auto p = pipe<int>();
    auto sig = collect(std::move(p.stream)).map(
            [](std::vector<int> const& v) { return v.size(); });

    DataContext ctx;
    auto data = initialize(ctx, sig);

    bool fired = false;
    auto conn = sig.unwrap().observe(ctx, data, [&] { fired = true; });

    p.handle.push(7);
    EXPECT_TRUE(fired);
}

// merge observes all of its children: a change in either leaf wakes the
// observer, once per external change.
TEST(Observe, throughMerge)
{
    auto pa = pipe<int>();
    auto pb = pipe<int>();
    auto sig = merge(collect(std::move(pa.stream)), collect(std::move(pb.stream)));

    DataContext ctx;
    auto data = initialize(ctx, sig);

    // A single observe connects both children. Each child is an independent
    // one-shot: pushing the first fires and disarms only its own leaf, leaving
    // the second still armed to fire on its push. So each external change fires
    // exactly once.
    int count = 0;
    auto conn = sig.unwrap().observe(ctx, data, [&] { ++count; });

    pa.handle.push(1);
    EXPECT_EQ(1, count);

    pb.handle.push(2);
    EXPECT_EQ(2, count);
}

// combine observes every child leaf.
TEST(Observe, throughCombine)
{
    auto pa = pipe<int>();
    auto pb = pipe<int>();

    std::vector<AnySignal<std::vector<int>>> children;
    children.push_back(collect(std::move(pa.stream)).eraseType());
    children.push_back(collect(std::move(pb.stream)).eraseType());
    auto sig = combine(std::move(children));

    DataContext ctx;
    auto data = initialize(ctx, sig);

    bool firedA = false;
    auto connA = sig.unwrap().observe(ctx, data, [&] { firedA = true; });
    pa.handle.push(1);
    EXPECT_TRUE(firedA);

    bool firedB = false;
    auto connB = sig.unwrap().observe(ctx, data, [&] { firedB = true; });
    pb.handle.push(2);
    EXPECT_TRUE(firedB);
}

// A conditional observes its condition and only the currently active branch;
// the inactive branch's leaf is not connected. Switching the condition and
// re-observing rewires onto the newly active branch.
TEST(Observe, throughConditional)
{
    auto pt = pipe<int>();
    auto pf = pipe<int>();
    auto cond = makeInput<bool>(true);

    auto sig = cond.signal.conditional(
            collect(std::move(pt.stream)),
            collect(std::move(pf.stream)));

    DataContext ctx;
    auto data = initialize(ctx, sig);

    // Active branch is the true branch: it fires, the inactive false branch
    // does not (it is neither initialized nor observed).
    bool firedTrue = false;
    bool firedFalse = false;
    auto conn = sig.unwrap().observe(ctx, data,
            [&] { firedTrue = true; });
    pt.handle.push(1);
    pf.handle.push(2);
    EXPECT_TRUE(firedTrue);
    EXPECT_FALSE(firedFalse);

    // Switch the active branch and drive an update so the conditional swaps in
    // the false branch's data.
    conn.disconnect();
    cond.handle.set(false);
    sig.unwrap().update(ctx, data, FrameInfo(1, {}));

    // Re-observing now registers on the false branch.
    auto conn2 = sig.unwrap().observe(ctx, data,
            [&] { firedFalse = true; });
    pf.handle.push(3);
    EXPECT_TRUE(firedFalse);
}

// join observes both the outer signal and the current inner signal, so an
// external change reachable through the inner signal wakes the observer.
TEST(Observe, throughJoin)
{
    auto p = pipe<int>();
    auto inner = collect(std::move(p.stream));
    auto sig = constant(inner).join();

    DataContext ctx;
    auto data = initialize(ctx, sig);

    bool fired = false;
    auto conn = sig.unwrap().observe(ctx, data, [&] { fired = true; });

    p.handle.push(42);
    EXPECT_TRUE(fired);
}

// join(ArraySignal) observes each element that is present when observe is
// called, so an external change at an element's leaf wakes the observer.
TEST(Observe, throughArrayJoin)
{
    auto p = pipe<int>();
    auto element = collect(std::move(p.stream)).eraseType();

    ArraySignal<AnySignal<std::vector<int>>> array(std::move(element));
    auto sig = join(array);

    DataContext ctx;
    auto data = initialize(ctx, sig);

    bool fired = false;
    auto conn = sig.unwrap().observe(ctx, data, [&] { fired = true; });

    p.handle.push(42);
    EXPECT_TRUE(fired);
}

// A plain value input is an external leaf: observing it and then set()ting a new
// value wakes the observer immediately, without evaluating the graph (the new
// value is only visible through evaluate() after an update()).
TEST(Observe, inputLeafFires)
{
    auto input = makeInput<int>(5);

    DataContext ctx;
    auto data = initialize(ctx, input.signal);

    bool fired = false;
    auto conn = input.signal.unwrap().observe(ctx, data, [&] { fired = true; });

    EXPECT_FALSE(fired);
    input.handle.set(10);
    EXPECT_TRUE(fired);

    // The wake did not evaluate: the value is still the old one until update().
    EXPECT_EQ(5, input.signal.unwrap().evaluate(ctx, data).get<0>());
    input.signal.unwrap().update(ctx, data, FrameInfo(1, {}));
    EXPECT_EQ(10, input.signal.unwrap().evaluate(ctx, data).get<0>());
}

// Input observe is one-shot too: a second set() wakes nothing until re-armed.
TEST(Observe, inputIsOneShot)
{
    auto input = makeInput<int>(0);

    DataContext ctx;
    auto data = initialize(ctx, input.signal);

    int count = 0;
    auto c1 = input.signal.unwrap().observe(ctx, data, [&] { ++count; });

    input.handle.set(1);
    EXPECT_EQ(1, count);

    input.handle.set(2);
    EXPECT_EQ(1, count);

    auto c2 = input.signal.unwrap().observe(ctx, data, [&] { ++count; });
    input.handle.set(3);
    EXPECT_EQ(2, count);
}

// Disconnecting an input observer unregisters it before set() can wake it.
TEST(Observe, inputDisconnectStops)
{
    auto input = makeInput<int>(0);

    DataContext ctx;
    auto data = initialize(ctx, input.signal);

    bool fired = false;
    auto conn = input.signal.unwrap().observe(ctx, data, [&] { fired = true; });
    conn.disconnect();

    input.handle.set(1);
    EXPECT_FALSE(fired);
}

// The input leaf wakes through a combinator as well: a set() at the input wakes
// an observer registered on the mapped node.
TEST(Observe, inputThroughMap)
{
    auto input = makeInput<int>(1);
    auto sig = input.signal.map([](int n) { return n * 2; });

    DataContext ctx;
    auto data = initialize(ctx, sig);

    bool fired = false;
    auto conn = sig.unwrap().observe(ctx, data, [&] { fired = true; });

    input.handle.set(4);
    EXPECT_TRUE(fired);
}

// ---------------------------------------------------------------------------
// SignalContext::observe -- the public entry point, registering a wakeup
// across every signal the context holds.
// ---------------------------------------------------------------------------

// A context over a collect leaf: observe() wakes on a stream emission, and the
// wake does not evaluate -- the value is not visible via evaluate<0>() until an
// update() applies it.
TEST(SignalContextObserve, collectWakesWithoutEvaluating)
{
    auto p = pipe<int>();
    auto c = makeSignalContext(collect(std::move(p.stream)));

    bool fired = false;
    auto conn = c.observe([&] { fired = true; });

    EXPECT_FALSE(fired);
    p.handle.push(42);
    EXPECT_TRUE(fired);

    // The wake did not evaluate the graph: the value is still pending.
    EXPECT_TRUE(c.evaluate<0>().get<0>().empty());

    c.update(FrameInfo(1, {}));
    EXPECT_EQ((std::vector<int>{ 42 }), c.evaluate<0>().get<0>());
}

// A context over a plain input leaf: observe() wakes on set(), exercising the
// input observe fix through the public API.
TEST(SignalContextObserve, inputWakesOnSet)
{
    auto input = makeInput<int>(5);
    auto c = makeSignalContext(input.signal);

    bool fired = false;
    auto conn = c.observe([&] { fired = true; });

    EXPECT_FALSE(fired);
    input.handle.set(10);
    EXPECT_TRUE(fired);
}

// The wakeup reaches leaves through combinators: a mapped input still wakes the
// context observer on set().
TEST(SignalContextObserve, throughMap)
{
    auto input = makeInput<int>(1);
    auto c = makeSignalContext(input.signal.map([](int n) { return n * 2; }));

    bool fired = false;
    auto conn = c.observe([&] { fired = true; });

    input.handle.set(4);
    EXPECT_TRUE(fired);
}

// observe() registers across every signal the context holds, so a change in
// either of two independent leaves wakes the observer.
TEST(SignalContextObserve, acrossAllSignals)
{
    auto pa = pipe<int>();
    auto pb = pipe<int>();
    auto c = makeSignalContext(
            collect(std::move(pa.stream)),
            collect(std::move(pb.stream)));

    int count = 0;
    auto conn = c.observe([&] { ++count; });

    pa.handle.push(1);
    EXPECT_EQ(1, count);

    pb.handle.push(2);
    EXPECT_EQ(2, count);
}

// The wakeup is one-shot: after firing, a second change does nothing until the
// context is observed again.
TEST(SignalContextObserve, isOneShot)
{
    auto input = makeInput<int>(0);
    auto c = makeSignalContext(input.signal);

    int count = 0;
    auto conn = c.observe([&] { ++count; });

    input.handle.set(1);
    EXPECT_EQ(1, count);

    // Not re-armed.
    input.handle.set(2);
    EXPECT_EQ(1, count);

    // Re-arming makes it fire again.
    auto conn2 = c.observe([&] { ++count; });
    input.handle.set(3);
    EXPECT_EQ(2, count);
}

// Dropping the returned connection unregisters the wakeup before the change.
TEST(SignalContextObserve, disconnectStopsFiring)
{
    auto p = pipe<int>();
    auto c = makeSignalContext(collect(std::move(p.stream)));

    bool fired = false;
    {
        auto conn = c.observe([&] { fired = true; });
    }

    p.handle.push(42);
    EXPECT_FALSE(fired);
}
