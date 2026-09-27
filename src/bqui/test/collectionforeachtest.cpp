#include <bqui/foreach.h>

#include <bqui/collection.h>
#include <bqui/widget/widget.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/signalcontext.h>

#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace bqui;
using bq::signal::FrameInfo;

namespace
{
    std::vector<size_t> collectionIds(Collection<std::string>& collection)
    {
        auto range = collection.rangeLock();
        std::vector<size_t> ids;
        for (auto i = range.begin(); i != range.end(); ++i)
            ids.push_back(i.getId());

        return ids;
    }

    std::vector<size_t> builtIds(
            std::vector<std::pair<size_t, widget::AnyWidget>> const& built)
    {
        std::vector<size_t> ids;
        for (auto const& entry : built)
            ids.push_back(entry.first);

        return ids;
    }
} // namespace

// The delegate runs once per item identity: neither a value update nor a
// reorder rebuilds anything, only a genuinely new id builds one more, and an
// erased id retires without rebuilding a survivor. Membership and order track
// the collection throughout.
TEST(collectionForEach, buildsOncePerIdentityAndFollowsMembership)
{
    Collection<std::string> items;
    {
        auto range = items.rangeLock();
        range.pushBack("a");
        range.pushBack("b");
    }

    auto ids = collectionIds(items);
    size_t const idA = ids[0];
    size_t const idB = ids[1];

    auto builds = std::make_shared<int>(0);

    auto c = makeSignalContext(forEach(items,
                [builds](bq::signal::AnySignal<std::string>, size_t)
                    -> widget::AnyWidget
                {
                    ++*builds;
                    return widget::makeWidget();
                }));

    EXPECT_EQ((std::vector<size_t>{ idA, idB }),
            builtIds(c.evaluate<0>().get<0>()));
    EXPECT_EQ(2, *builds);

    // A value update rebuilds nothing and leaves membership unchanged.
    {
        auto range = items.rangeLock();
        range.update(range.findId(idB), "b2");
    }
    c.update(FrameInfo(1, {}));
    EXPECT_EQ((std::vector<size_t>{ idA, idB }),
            builtIds(c.evaluate<0>().get<0>()));
    EXPECT_EQ(2, *builds);

    // A swap keeps both identities and reports the new order.
    {
        auto range = items.rangeLock();
        range.swap(range.findId(idA), range.findId(idB));
    }
    c.update(FrameInfo(2, {}));
    EXPECT_EQ((std::vector<size_t>{ idB, idA }),
            builtIds(c.evaluate<0>().get<0>()));
    EXPECT_EQ(2, *builds);

    // As does a move.
    {
        auto range = items.rangeLock();
        range.move(range.findId(idA), range.begin());
    }
    c.update(FrameInfo(3, {}));
    EXPECT_EQ((std::vector<size_t>{ idA, idB }),
            builtIds(c.evaluate<0>().get<0>()));
    EXPECT_EQ(2, *builds);

    // Only the new id builds one more.
    {
        auto range = items.rangeLock();
        range.pushBack("c");
    }
    size_t const idC = collectionIds(items).back();
    c.update(FrameInfo(4, {}));
    EXPECT_EQ((std::vector<size_t>{ idA, idB, idC }),
            builtIds(c.evaluate<0>().get<0>()));
    EXPECT_EQ(3, *builds);

    // Erasing a survivor rebuilds nothing.
    {
        auto range = items.rangeLock();
        range.eraseWithId(idB);
    }
    c.update(FrameInfo(5, {}));
    EXPECT_EQ((std::vector<size_t>{ idA, idC }),
            builtIds(c.evaluate<0>().get<0>()));
    EXPECT_EQ(3, *builds);
}

// The sharp end of build-once: a survivor's built value is never destroyed by a
// value update or a reorder, the changed value reaches it through its own value
// signal, and only the erased identity's value is retired. A weak reference to a
// marker owned by each built value makes the destruction observable, as counting
// builds alone cannot.
TEST(collectionForEach, valueFlowsAndSurvivorsPersistThroughChange)
{
    using Item = std::pair<size_t, std::string>;

    Collection<std::string> items;
    {
        auto range = items.rangeLock();
        range.pushBack("a");
        range.pushBack("b");
    }

    auto ids = collectionIds(items);
    size_t const idA = ids[0];
    size_t const idB = ids[1];

    auto builds = std::make_shared<int>(0);
    auto markers = std::make_shared<std::vector<std::weak_ptr<int>>>();

    auto array = bq::signal::forEach(
            collectionSignal(items),
            [](Item const& item)
            {
                return item.first;
            },
            [builds, markers](size_t, bq::signal::AnySignal<Item> item)
            {
                ++*builds;

                // Lives exactly as long as what the delegate returns.
                auto marker = std::make_shared<int>(0);
                markers->push_back(marker);

                return bq::signal::AnySignal<std::string>(item.map(
                            [marker](Item const& i)
                            {
                                return i.second;
                            }));
            });

    auto c = makeSignalContext(bq::signal::join(std::move(array)));

    EXPECT_EQ((std::vector<std::string>{ "a", "b" }),
            c.evaluate<0>().get<0>());
    EXPECT_EQ(2, *builds);
    ASSERT_EQ(2u, markers->size());
    EXPECT_FALSE((*markers)[0].expired());
    EXPECT_FALSE((*markers)[1].expired());

    // A value update flows through the item's signal without rebuilding.
    {
        auto range = items.rangeLock();
        range.update(range.findId(idA), "a2");
    }
    c.update(FrameInfo(1, {}));
    EXPECT_EQ((std::vector<std::string>{ "a2", "b" }),
            c.evaluate<0>().get<0>());
    EXPECT_EQ(2, *builds);
    EXPECT_FALSE((*markers)[0].expired());
    EXPECT_FALSE((*markers)[1].expired());

    // A reorder keeps both identities and their built values.
    {
        auto range = items.rangeLock();
        range.swap(range.findId(idA), range.findId(idB));
    }
    c.update(FrameInfo(2, {}));
    EXPECT_EQ((std::vector<std::string>{ "b", "a2" }),
            c.evaluate<0>().get<0>());
    EXPECT_EQ(2, *builds);
    EXPECT_FALSE((*markers)[0].expired());
    EXPECT_FALSE((*markers)[1].expired());

    // Erasing a survivor retires exactly its built value.
    {
        auto range = items.rangeLock();
        range.eraseWithId(idA);
    }
    c.update(FrameInfo(3, {}));
    EXPECT_EQ((std::vector<std::string>{ "b" }), c.evaluate<0>().get<0>());
    EXPECT_EQ(2, *builds);
    EXPECT_TRUE((*markers)[0].expired());
    EXPECT_FALSE((*markers)[1].expired());

    // A new id builds exactly one more value.
    {
        auto range = items.rangeLock();
        range.pushBack("c");
    }
    c.update(FrameInfo(4, {}));
    EXPECT_EQ((std::vector<std::string>{ "b", "c" }),
            c.evaluate<0>().get<0>());
    EXPECT_EQ(3, *builds);
    ASSERT_EQ(3u, markers->size());
    EXPECT_FALSE((*markers)[2].expired());
}

// Separate signals made from one collection, copies of one, and contexts
// evaluating them all carry the same snapshot for each change.
TEST(collectionForEach, signalsOfOneCollectionAgree)
{
    using Items = std::vector<std::pair<size_t, std::string>>;

    Collection<std::string> items;
    {
        auto range = items.rangeLock();
        range.pushBack("a");
        range.pushBack("b");
    }

    auto first = collectionSignal(items);
    auto second = collectionSignal(items);
    auto copy = first;

    auto together = makeSignalContext(first, second);
    auto apart = makeSignalContext(copy);

    auto expectAgree = [&]()
        {
            Items const value = together.evaluate<0>().get<0>();
            EXPECT_EQ(value, together.evaluate<1>().get<0>());
            EXPECT_EQ(value, apart.evaluate<0>().get<0>());
            return value;
        };

    auto const initial = expectAgree();
    ASSERT_EQ(2u, initial.size());
    EXPECT_EQ(collectionIds(items),
            (std::vector<size_t>{ initial[0].first, initial[1].first }));

    {
        auto range = items.rangeLock();
        range.pushBack("c");
        range.update(range.begin(), "a2");
    }
    together.update(FrameInfo(1, {}));
    apart.update(FrameInfo(1, {}));
    EXPECT_EQ(3u, expectAgree().size());

    {
        auto range = items.rangeLock();
        range.sort(std::greater<std::string>());
    }
    together.update(FrameInfo(2, {}));
    apart.update(FrameInfo(2, {}));
    auto const sorted = expectAgree();
    ASSERT_EQ(3u, sorted.size());
    EXPECT_EQ("c", sorted[0].second);
    EXPECT_EQ("b", sorted[1].second);
    EXPECT_EQ("a2", sorted[2].second);

    // A signal made after the changes starts from the same current snapshot.
    auto late = makeSignalContext(collectionSignal(items));
    EXPECT_EQ(sorted, late.evaluate<0>().get<0>());
}

// A signal made before a change and evaluated only afterwards sees the change,
// and a collection outliving every signal can be observed again.
TEST(collectionForEach, subscriptionFollowsSignalLifetime)
{
    Collection<std::string> items;

    {
        auto signal = collectionSignal(items);
        {
            auto range = items.rangeLock();
            range.pushBack("a");
        }

        auto c = makeSignalContext(signal);
        ASSERT_EQ(1u, c.evaluate<0>().get<0>().size());
        EXPECT_EQ("a", c.evaluate<0>().get<0>()[0].second);
    }

    {
        auto range = items.rangeLock();
        range.pushBack("b");
    }

    auto c = makeSignalContext(collectionSignal(items));
    ASSERT_EQ(2u, c.evaluate<0>().get<0>().size());

    {
        auto range = items.rangeLock();
        range.eraseWithId(range.begin().getId());
    }
    c.update(FrameInfo(1, {}));
    ASSERT_EQ(1u, c.evaluate<0>().get<0>().size());
    EXPECT_EQ("b", c.evaluate<0>().get<0>()[0].second);
}

// Two forEach over one collection build the same identities in the same order.
TEST(collectionForEach, repeatedForEachAgree)
{
    Collection<std::string> items;
    {
        auto range = items.rangeLock();
        range.pushBack("a");
        range.pushBack("b");
    }

    auto delegate = [](bq::signal::AnySignal<std::string>, size_t)
        -> widget::AnyWidget
        {
            return widget::makeWidget();
        };

    auto c = makeSignalContext(forEach(items, delegate),
            forEach(items, delegate));

    EXPECT_EQ(collectionIds(items), builtIds(c.evaluate<0>().get<0>()));
    EXPECT_EQ(builtIds(c.evaluate<0>().get<0>()),
            builtIds(c.evaluate<1>().get<0>()));

    size_t const idB = collectionIds(items).back();
    {
        auto range = items.rangeLock();
        range.pushFront("c");
        range.eraseWithId(idB);
    }
    c.update(FrameInfo(1, {}));

    EXPECT_EQ(collectionIds(items), builtIds(c.evaluate<0>().get<0>()));
    EXPECT_EQ(builtIds(c.evaluate<0>().get<0>()),
            builtIds(c.evaluate<1>().get<0>()));
}

// Signals of one collection agree on every pass even while another thread
// mutates the collection during evaluation.
TEST(collectionForEach, signalsAgreeUnderConcurrentMutation)
{
    Collection<int> items;

    auto c = makeSignalContext(collectionSignal(items),
            collectionSignal(items));

    std::atomic<bool> done{ false };
    std::thread writer([&items, &done]()
        {
            for (int i = 0; i < 2000; ++i)
            {
                auto range = items.rangeLock();
                range.pushBack(i);
                if (range.size() > 8)
                    range.erase(range.begin());
            }
            done = true;
        });

    uint64_t frame = 0;
    bool agreed = true;
    while (!done && agreed)
    {
        c.update(FrameInfo(++frame, {}));
        agreed = c.evaluate<0>().get<0>() == c.evaluate<1>().get<0>();
    }

    writer.join();
    EXPECT_TRUE(agreed);

    c.update(FrameInfo(++frame, {}));
    auto const last = c.evaluate<0>().get<0>();
    EXPECT_EQ(last, c.evaluate<1>().get<0>());
    ASSERT_EQ(8u, last.size());
    EXPECT_EQ(1999, last.back().second);
}
