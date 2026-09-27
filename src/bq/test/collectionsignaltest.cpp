#include <bq/signal/collectionsignal.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/constant.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/input.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace bq::signal;

namespace
{
    using Built = std::pair<size_t, std::string>;
    using StringSnapshot = Collection<std::string>::SnapshotPtr;

    std::vector<size_t> collectionIds(Collection<std::string> const& collection)
    {
        std::vector<size_t> ids;
        auto range = collection.crangeLock();
        for (auto i = range.begin(); i != range.end(); ++i)
            ids.push_back(i.getId());

        return ids;
    }

    std::vector<size_t> builtIds(std::vector<Built> const& built)
    {
        std::vector<size_t> ids;
        for (auto const& entry : built)
            ids.push_back(entry.first);

        return ids;
    }

    // One (id, value) signal per item, fanned back in.
    AnySignal<std::vector<Built>> idsAndValues(
            Collection<std::string> const& collection,
            std::shared_ptr<int> builds = std::make_shared<int>(0))
    {
        return join(forEach(collection,
                    [builds](AnySignal<std::string> value, size_t id)
                    {
                        ++*builds;
                        return AnySignal<Built>(value.map(
                                    [id](std::string const& v)
                                    {
                                        return Built(id, v);
                                    }));
                    }));
    }

    Collection<std::string> makeCollection(std::vector<std::string> items)
    {
        Collection<std::string> collection;
        auto range = collection.rangeLock();
        for (auto& item : items)
            range.pushBack(std::move(item));

        return collection;
    }
} // namespace

// The delegate runs once per item id: neither a value update nor a reorder
// rebuilds anything, only a new id builds one more, and an erased id retires
// without rebuilding a survivor. Membership and order track the collection.
TEST(collectionSignal, buildsOncePerIdentityAndFollowsMembership)
{
    auto items = makeCollection({ "a", "b" });

    auto ids = collectionIds(items);
    size_t const idA = ids[0];
    size_t const idB = ids[1];

    auto builds = std::make_shared<int>(0);
    auto c = makeSignalContext(idsAndValues(items, builds));

    EXPECT_EQ((std::vector<size_t>{ idA, idB }),
            builtIds(c.evaluate<0>().get<0>()));
    EXPECT_EQ(2, *builds);

    {
        auto range = items.rangeLock();
        range.update(range.findId(idB), "b2");
    }
    c.update(FrameInfo(1, {}));
    EXPECT_EQ((std::vector<Built>{ { idA, "a" }, { idB, "b2" } }),
            c.evaluate<0>().get<0>());
    EXPECT_EQ(2, *builds);

    {
        auto range = items.rangeLock();
        range.swap(range.findId(idA), range.findId(idB));
    }
    c.update(FrameInfo(2, {}));
    EXPECT_EQ((std::vector<size_t>{ idB, idA }),
            builtIds(c.evaluate<0>().get<0>()));
    EXPECT_EQ(2, *builds);

    {
        auto range = items.rangeLock();
        range.move(range.findId(idA), range.begin());
    }
    c.update(FrameInfo(3, {}));
    EXPECT_EQ((std::vector<size_t>{ idA, idB }),
            builtIds(c.evaluate<0>().get<0>()));
    EXPECT_EQ(2, *builds);

    items.rangeLock().pushBack("c");
    size_t const idC = collectionIds(items).back();
    c.update(FrameInfo(4, {}));
    EXPECT_EQ((std::vector<size_t>{ idA, idB, idC }),
            builtIds(c.evaluate<0>().get<0>()));
    EXPECT_EQ(3, *builds);

    items.rangeLock().eraseWithId(idB);
    c.update(FrameInfo(5, {}));
    EXPECT_EQ((std::vector<Built>{ { idA, "a" }, { idC, "c" } }),
            c.evaluate<0>().get<0>());
    EXPECT_EQ(3, *builds);
}

// A survivor's built value is never destroyed by a value update or a reorder,
// and only the erased identity's value is retired.
TEST(collectionSignal, valueFlowsAndSurvivorsPersistThroughChange)
{
    auto items = makeCollection({ "a", "b" });

    auto ids = collectionIds(items);
    size_t const idA = ids[0];
    size_t const idB = ids[1];

    auto markers = std::make_shared<std::vector<std::weak_ptr<int>>>();

    auto c = makeSignalContext(join(forEach(items,
                    [markers](AnySignal<std::string> value, size_t)
                    {
                        auto marker = std::make_shared<int>(0);
                        markers->push_back(marker);

                        return AnySignal<std::string>(value.map(
                                    [marker](std::string const& v)
                                    {
                                        return v;
                                    }));
                    })));

    EXPECT_EQ((std::vector<std::string>{ "a", "b" }),
            c.evaluate<0>().get<0>());
    ASSERT_EQ(2u, markers->size());

    {
        auto range = items.rangeLock();
        range.update(range.findId(idA), "a2");
    }
    c.update(FrameInfo(1, {}));
    EXPECT_EQ((std::vector<std::string>{ "a2", "b" }),
            c.evaluate<0>().get<0>());

    {
        auto range = items.rangeLock();
        range.swap(range.findId(idA), range.findId(idB));
    }
    c.update(FrameInfo(2, {}));
    EXPECT_EQ((std::vector<std::string>{ "b", "a2" }),
            c.evaluate<0>().get<0>());
    ASSERT_EQ(2u, markers->size());
    EXPECT_FALSE((*markers)[0].expired());
    EXPECT_FALSE((*markers)[1].expired());

    items.rangeLock().eraseWithId(idA);
    c.update(FrameInfo(3, {}));
    EXPECT_EQ((std::vector<std::string>{ "b" }), c.evaluate<0>().get<0>());
    EXPECT_TRUE((*markers)[0].expired());
    EXPECT_FALSE((*markers)[1].expired());
}

// A single-item update changes that item's signal and no other.
TEST(collectionSignal, itemSignalsDidChangeOnlyForTheirItem)
{
    auto items = makeCollection({ "a", "b" });
    auto ids = collectionIds(items);

    auto array = forEach(items,
            [](AnySignal<std::string> value, size_t)
            {
                return value;
            });

    auto elements = makeSignalContext(array.elements());
    auto const& built = elements.evaluate<0>().get<0>();
    ASSERT_EQ(2u, built.size());

    // Instantiate the two item signals side by side in one context.
    auto c = makeSignalContext(built[0].value, built[1].value);
    EXPECT_EQ("a", c.evaluate<0>().get<0>());
    EXPECT_EQ("b", c.evaluate<1>().get<0>());

    {
        auto range = items.rangeLock();
        range.update(range.findId(ids[1]), "b2");
    }
    c.update(FrameInfo(1, {}));
    EXPECT_FALSE(c.didChange<0>());
    EXPECT_TRUE(c.didChange<1>());
    EXPECT_EQ("b2", c.evaluate<1>().get<0>());

    // A reorder or an insert is not a change of either item.
    {
        auto range = items.rangeLock();
        range.swap(range.begin(), range.begin() + 1);
        range.pushBack("c");
    }
    c.update(FrameInfo(2, {}));
    EXPECT_FALSE(c.didChange<0>());
    EXPECT_FALSE(c.didChange<1>());

    // An erased item keeps its last value and stops changing.
    items.rangeLock().eraseWithId(ids[0]);
    c.update(FrameInfo(3, {}));
    EXPECT_FALSE(c.didChange<0>());
    EXPECT_EQ("a", c.evaluate<0>().get<0>());
}

// Many changes in one transaction are one generation and one change of the
// generation signal.
TEST(collectionSignal, oneTransactionIsOneChange)
{
    Collection<int> items;
    auto c = makeSignalContext(generationSignal(items));
    EXPECT_EQ(0u, c.evaluate<0>().get<0>());

    {
        auto range = items.rangeLock();
        for (int i = 0; i < 10; ++i)
            range.pushBack(i);
        range.update(range.begin(), 100);
    }

    c.update(FrameInfo(1, {}));
    EXPECT_TRUE(c.didChange<0>());
    EXPECT_EQ(1u, c.evaluate<0>().get<0>());

    c.update(FrameInfo(2, {}));
    EXPECT_FALSE(c.didChange<0>());
}

// Every signal over one collection in one context, and both of two forEach
// over it, see the same snapshot within a frame, even when a new generation is
// published while the context is part-way through its update.
TEST(collectionSignal, oneContextSeesOneSnapshotPerFrame)
{
    auto items = makeCollection({ "a", "b" });

    // Evaluated right after its own update, between its siblings' updates,
    // and publishes another generation from there when armed.
    auto armed = std::make_shared<bool>(false);
    auto writer = std::make_shared<Collection<std::string>>(items);
    auto intruder = generationSignal(items).map(
            [armed, writer](uint64_t generation)
            {
                if (*armed)
                {
                    *armed = false;
                    writer->rangeLock().pushBack("intruder");
                }

                return generation;
            });

    auto c = makeSignalContext(snapshotSignal(items), idsAndValues(items),
            intruder, snapshotSignal(items), idsAndValues(items));

    auto expectAgree = [&]()
        {
            auto const& snapshot = c.evaluate<0>().get<0>();
            EXPECT_EQ(snapshot, c.evaluate<3>().get<0>());
            EXPECT_EQ(snapshot->generation, c.evaluate<2>().get<0>());

            std::vector<Built> expected;
            for (auto const& item : snapshot->items)
                expected.emplace_back(item.id, *item.value);

            EXPECT_EQ(expected, c.evaluate<1>().get<0>());
            EXPECT_EQ(expected, c.evaluate<4>().get<0>());
        };

    expectAgree();

    {
        auto range = items.rangeLock();
        range.pushFront("c");
        range.update(range.begin() + 1, "a2");
    }

    *armed = true;
    c.update(FrameInfo(1, {}));
    EXPECT_FALSE(*armed);
    EXPECT_EQ(3u, items.generation());
    expectAgree();
    EXPECT_EQ(2u, c.evaluate<0>().get<0>()->generation);

    // The intruding generation arrives at the next frame, for all of them.
    c.update(FrameInfo(2, {}));
    expectAgree();
    EXPECT_EQ(3u, c.evaluate<0>().get<0>()->generation);
    EXPECT_EQ(4u, c.evaluate<1>().get<0>().size());

    items.rangeLock().sort();
    c.update(FrameInfo(3, {}));
    expectAgree();
}

// Separate contexts are independent: each loads the latest snapshot at its
// own frames.
TEST(collectionSignal, contextsAdvanceIndependently)
{
    auto items = makeCollection({ "a" });

    auto first = makeSignalContext(generationSignal(items));
    items.rangeLock().pushBack("b");
    auto second = makeSignalContext(generationSignal(items));

    EXPECT_EQ(1u, first.evaluate<0>().get<0>());
    EXPECT_EQ(2u, second.evaluate<0>().get<0>());

    first.update(FrameInfo(1, {}));
    EXPECT_EQ(2u, first.evaluate<0>().get<0>());
}

// A signal instantiated in the middle of a frame reads the snapshot its
// context is already on, and does not inherit a sibling's change.
TEST(collectionSignal, lateSubscriberSharesTheFrameSnapshot)
{
    auto items = makeCollection({ "a" });

    auto late = makeInput(AnySignal<StringSnapshot>(
                constant(StringSnapshot())));

    auto c = makeSignalContext(snapshotSignal(items), late.signal.join());

    items.rangeLock().pushBack("b");
    late.handle.set(snapshotSignal(items));

    c.update(FrameInfo(1, {}));
    EXPECT_TRUE(c.didChange<0>());
    ASSERT_TRUE(c.evaluate<1>().get<0>());
    EXPECT_EQ(c.evaluate<0>().get<0>(), c.evaluate<1>().get<0>());
    EXPECT_EQ(2u, c.evaluate<1>().get<0>()->generation);

    c.update(FrameInfo(2, {}));
    EXPECT_FALSE(c.didChange<0>());
    EXPECT_FALSE(c.didChange<1>());

    items.rangeLock().pushBack("c");
    c.update(FrameInfo(3, {}));
    EXPECT_TRUE(c.didChange<1>());
    EXPECT_EQ(c.evaluate<0>().get<0>(), c.evaluate<1>().get<0>());
}

// A publish wakes an observing context, and the snapshot a context holds lives
// exactly as long as the signals that read it.
TEST(collectionSignal, observerAndSnapshotLifetimeFollowTheSignals)
{
    auto items = makeCollection({ "a" });
    std::weak_ptr<Collection<std::string>::Snapshot const> held =
        items.snapshot();

    auto wakes = std::make_shared<std::atomic<int>>(0);
    {
        auto c = makeSignalContext(idsAndValues(items));
        c.observe([wakes]()
                {
                    ++*wakes;
                });

        items.rangeLock().pushBack("b");
        EXPECT_EQ(1, wakes->load());

        // The context still holds the snapshot it is on.
        EXPECT_FALSE(held.expired());

        c.update(FrameInfo(1, {}));
        EXPECT_TRUE(held.expired());
        held = items.snapshot();
        items.rangeLock().pushBack("c");
        EXPECT_EQ(2, wakes->load());
        EXPECT_FALSE(held.expired());
    }

    EXPECT_TRUE(held.expired());

    items.rangeLock().pushBack("d");
    EXPECT_EQ(2, wakes->load());
}

// Signals of one collection agree on every pass, and match the collection
// contents, while another thread mutates the collection during evaluation.
TEST(collectionSignal, signalsAgreeUnderConcurrentMutation)
{
    Collection<std::string> items;

    auto c = makeSignalContext(snapshotSignal(items), idsAndValues(items),
            idsAndValues(items));

    std::atomic<bool> done{ false };
    std::thread writer([&items, &done]()
        {
            for (int i = 0; i < 2000; ++i)
            {
                auto range = items.rangeLock();
                range.pushBack(std::to_string(i));
                if (range.size() > 8)
                    range.erase(range.begin());
                if (i % 3 == 0)
                    range.update(range.begin(), "u" + std::to_string(i));
            }
            done = true;
        });

    uint64_t frame = 0;
    bool agreed = true;
    auto check = [&]()
        {
            auto const& snapshot = c.evaluate<0>().get<0>();
            std::vector<Built> expected;
            for (auto const& item : snapshot->items)
                expected.emplace_back(item.id, *item.value);

            return expected == c.evaluate<1>().get<0>()
                && expected == c.evaluate<2>().get<0>();
        };

    while (!done && agreed)
    {
        c.update(FrameInfo(++frame, {}));
        agreed = check();
    }

    writer.join();
    EXPECT_TRUE(agreed);

    c.update(FrameInfo(++frame, {}));
    EXPECT_TRUE(check());
    auto const last = c.evaluate<1>().get<0>();
    ASSERT_EQ(8u, last.size());
    EXPECT_EQ("1999", last.back().second);
}
