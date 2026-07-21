#include <bq/signal/detail/pick.h>

#include <bq/signal/datacontext.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/input.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace bq::signal;
using bq::signal::detail::pick;
using bq::signal::detail::requirePresent;
using bq::signal::detail::shareKeyed;

namespace
{
    struct Item
    {
        std::string key;
        int value;
    };

    auto itemKey = [](Item const& item)
    {
        return item.key;
    };

    // Counts how many times the source is evaluated, from inside the shared
    // subgraph so that the count is a count of evaluation passes.
    template <typename TSignal>
    auto countedShareKeyed(TSignal source, std::shared_ptr<int> passes)
    {
        return shareKeyed(
                std::move(source).map([passes](std::vector<Item> const& items)
                    {
                        ++*passes;
                        return items;
                    }),
                itemKey);
    }
} // namespace

// A pick follows its own key's value while the rest of the membership moves
// around it. Each membership change carries a change to the picked element as
// well, so a pick that stopped following would be caught rather than agreeing
// by accident.
TEST(pick, followsItsKeyAcrossMembershipChanges)
{
    auto input = makeInput(std::vector<Item>{ { "a", 1 }, { "b", 2 } });

    auto context = makeSignalContext(
            pick(shareKeyed(input.signal, itemKey), std::string("b")));

    EXPECT_EQ(2, context.evaluate<0>().get<0>()->value);

    // An element is inserted ahead of it.
    input.handle.set(std::vector<Item>{ { "c", 3 }, { "a", 1 }, { "b", 20 } });
    context.update(FrameInfo(1, {}));
    EXPECT_EQ(20, context.evaluate<0>().get<0>()->value);

    // Another element is removed.
    input.handle.set(std::vector<Item>{ { "c", 3 }, { "b", 30 } });
    context.update(FrameInfo(2, {}));
    EXPECT_EQ(30, context.evaluate<0>().get<0>()->value);

    // It moves to the other end.
    input.handle.set(std::vector<Item>{ { "b", 40 }, { "c", 3 } });
    context.update(FrameInfo(3, {}));
    EXPECT_EQ(40, context.evaluate<0>().get<0>()->value);

    // A pure reorder is invisible to a pick: the keyed source is the same map
    // either way.
    input.handle.set(std::vector<Item>{ { "c", 3 }, { "b", 40 } });
    context.update(FrameInfo(4, {}));
    EXPECT_EQ(40, context.evaluate<0>().get<0>()->value);
}

// A key that is not in the source has no value. It is reported as such rather
// than as a stale one, and the pick follows the key again if it comes back.
TEST(pick, hasNoValueWhileItsKeyIsAbsent)
{
    auto input = makeInput(std::vector<Item>{ { "a", 1 }, { "b", 2 } });

    auto context = makeSignalContext(
            pick(shareKeyed(input.signal, itemKey), std::string("b")));

    EXPECT_EQ(2, context.evaluate<0>().get<0>()->value);

    input.handle.set(std::vector<Item>{ { "a", 1 } });
    context.update(FrameInfo(1, {}));
    EXPECT_FALSE(context.evaluate<0>().get<0>().has_value());

    input.handle.set(std::vector<Item>{ { "a", 10 } });
    context.update(FrameInfo(2, {}));
    EXPECT_FALSE(context.evaluate<0>().get<0>().has_value());

    // The key returns and is picked up again.
    input.handle.set(std::vector<Item>{ { "a", 10 }, { "b", 30 } });
    context.update(FrameInfo(3, {}));
    EXPECT_EQ(30, context.evaluate<0>().get<0>()->value);
}

// A consumer that needs the element to be there says so, and gets a loud
// failure rather than a quiet one when it is not.
TEST(pick, requirePresentThrowsOnAnAbsentKey)
{
    auto input = makeInput(std::vector<Item>{ { "a", 1 }, { "b", 2 } });

    auto description = requirePresent(
            pick(shareKeyed(input.signal, itemKey), std::string("b")));
    auto& sig = description.unwrap();

    DataContext context;

    auto data = sig.initialize(context, FrameInfo(0, {}));
    EXPECT_EQ(2, sig.evaluate(context, data).get<0>().value);

    input.handle.set(std::vector<Item>{ { "a", 1 } });
    sig.update(context, data, FrameInfo(1, {}));

    EXPECT_THROW(sig.evaluate(context, data), std::runtime_error);
}

// A pick reports a change whenever the source changes, even when the picked
// element did not move. Suppressing the repeats is the caller's choice, made
// with the facility bq already has for it.
TEST(pick, reportsAChangeOfAnyElementAndCheckSuppressesIt)
{
    auto input = makeInput(std::vector<Item>{ { "a", 1 }, { "b", 2 } });
    auto shared = shareKeyed(input.signal, itemKey);

    auto value = [](Item const& item)
    {
        return item.value;
    };

    auto context = makeSignalContext(
            requirePresent(pick(shared, std::string("b"))).map(value),
            requirePresent(pick(shared, std::string("b")))
                .map(value)
                .check());

    // Only "a" moves.
    input.handle.set(std::vector<Item>{ { "a", 100 }, { "b", 2 } });
    context.update(FrameInfo(1, {}));

    EXPECT_TRUE(context.didChange<0>());
    EXPECT_FALSE(context.didChange<1>());
    EXPECT_EQ(2, context.evaluate<0>().get<0>());
    EXPECT_EQ(2, context.evaluate<1>().get<0>());
}

// State lives in the SignalContext, so two contexts over one description share
// nothing: each evaluates the source for itself, and driving one must leave the
// other exactly where it was.
TEST(pick, twoContextsOverOneDescriptionAreIndependent)
{
    auto input = makeInput(std::vector<Item>{ { "a", 1 }, { "b", 2 } });
    auto passes = std::make_shared<int>(0);

    auto description = pick(countedShareKeyed(input.signal, passes),
            std::string("b"));

    auto first = makeSignalContext(description);
    auto second = makeSignalContext(description);

    EXPECT_EQ(2, *passes);
    EXPECT_EQ(2, first.evaluate<0>().get<0>()->value);
    EXPECT_EQ(2, second.evaluate<0>().get<0>()->value);

    // The key leaves, and only the first context is driven. The second has not
    // seen the change at all.
    input.handle.set(std::vector<Item>{ { "a", 1 } });
    first.update(FrameInfo(1, {}));
    EXPECT_EQ(3, *passes);
    EXPECT_FALSE(first.evaluate<0>().get<0>().has_value());
    EXPECT_EQ(2, second.evaluate<0>().get<0>()->value);

    // The key returns with a new value, still only for the first context.
    input.handle.set(std::vector<Item>{ { "a", 1 }, { "b", 30 } });
    first.update(FrameInfo(2, {}));
    EXPECT_EQ(30, first.evaluate<0>().get<0>()->value);
    EXPECT_EQ(2, second.evaluate<0>().get<0>()->value);

    // Driving the second context moves it alone. It never saw the value the
    // first context passed through on the way.
    input.handle.set(std::vector<Item>{ { "a", 1 }, { "b", 40 } });
    second.update(FrameInfo(1, {}));
    EXPECT_EQ(40, second.evaluate<0>().get<0>()->value);
    EXPECT_EQ(30, first.evaluate<0>().get<0>()->value);
}

// One description instantiated twice into one context finds the state of the
// shared source it already has there: the source is evaluated once per pass
// rather than once per consumer.
TEST(pick, oneDescriptionTwiceInOneContextSharesTheSource)
{
    auto input = makeInput(std::vector<Item>{ { "a", 1 }, { "b", 2 } });
    auto passes = std::make_shared<int>(0);

    auto description = pick(countedShareKeyed(input.signal, passes),
            std::string("b"));
    auto& sig = description.unwrap();

    DataContext context;

    auto first = sig.initialize(context, FrameInfo(0, {}));
    EXPECT_EQ(1, *passes);
    EXPECT_EQ(2, sig.evaluate(context, first).get<0>()->value);

    // A second instantiation into the same context adds no evaluation of its
    // own, at initialization or afterwards.
    auto second = sig.initialize(context, FrameInfo(0, {}));
    EXPECT_EQ(1, *passes);
    EXPECT_EQ(2, sig.evaluate(context, second).get<0>()->value);

    input.handle.set(std::vector<Item>{ { "a", 1 }, { "b", 20 } });
    sig.update(context, first, FrameInfo(1, {}));
    sig.update(context, second, FrameInfo(1, {}));

    EXPECT_EQ(2, *passes);
    EXPECT_EQ(20, sig.evaluate(context, first).get<0>()->value);
    EXPECT_EQ(20, sig.evaluate(context, second).get<0>()->value);
}

// The same sharing along the path a SignalContext actually takes: two entries
// over one description, one source evaluation per pass, both entries agreeing.
TEST(pick, twoContextEntriesOverOneDescriptionMoveTogether)
{
    auto input = makeInput(std::vector<Item>{ { "a", 1 }, { "b", 2 } });
    auto passes = std::make_shared<int>(0);

    auto description = pick(countedShareKeyed(input.signal, passes),
            std::string("b"));

    auto context = makeSignalContext(description, description);

    EXPECT_EQ(1, *passes);
    EXPECT_EQ(2, context.evaluate<0>().get<0>()->value);
    EXPECT_EQ(2, context.evaluate<1>().get<0>()->value);

    input.handle.set(std::vector<Item>{ { "a", 1 }, { "b", 20 } });
    context.update(FrameInfo(1, {}));

    EXPECT_EQ(2, *passes);
    EXPECT_EQ(20, context.evaluate<0>().get<0>()->value);
    EXPECT_EQ(20, context.evaluate<1>().get<0>()->value);

    input.handle.set(std::vector<Item>{ { "a", 1 } });
    context.update(FrameInfo(2, {}));

    EXPECT_EQ(3, *passes);
    EXPECT_FALSE(context.evaluate<0>().get<0>().has_value());
    EXPECT_FALSE(context.evaluate<1>().get<0>().has_value());
}

// The per-context state of a graph that nothing holds any more expires while
// its entries stay in the DataContext's map. Initializing the same description
// into the same context again must start over from the current source rather
// than trip over the stale entries.
TEST(pick, canBeInitializedAgainAfterItsStateExpired)
{
    auto input = makeInput(std::vector<Item>{ { "a", 1 }, { "b", 2 } });

    auto description = pick(shareKeyed(input.signal, itemKey),
            std::string("b"));
    auto& sig = description.unwrap();

    DataContext context;

    {
        auto data = sig.initialize(context, FrameInfo(0, {}));
        EXPECT_EQ(2, sig.evaluate(context, data).get<0>()->value);
    }

    input.handle.set(std::vector<Item>{ { "a", 1 }, { "b", 5 } });

    auto data = sig.initialize(context, FrameInfo(1, {}));
    EXPECT_EQ(5, sig.evaluate(context, data).get<0>()->value);
}
