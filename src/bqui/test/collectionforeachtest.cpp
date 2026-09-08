#include <bqui/foreach.h>

#include <bqui/collection.h>
#include <bqui/widget/widget.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/signalcontext.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>
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
