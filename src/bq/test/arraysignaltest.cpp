#include <bq/signal/arraysignal.h>

#include <bq/signal/constant.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/input.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>
#include <bq/signal/signaltraits.h>

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace bq::signal;

static_assert(checkSignal<detail::ArrayConstant<int>>());

static_assert(checkSignal<detail::ArrayJoin<int>>());

static_assert(checkSignal<detail::ArrayOnce<
        int,
        std::function<int(int const&)>,
        std::function<int(int const&, int const&)>,
        std::function<detail::ArrayId(int const&)>
        >>());

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

    std::vector<Item> items(std::initializer_list<Item> list)
    {
        return std::vector<Item>(list);
    }

    // The value of every element, in membership order. Fanning an array of
    // plain values in means giving each one a signal first, which is what a
    // map once per identity does.
    template <typename T>
    AnySignal<std::vector<T>> values(ArraySignal<T> array)
    {
        return join(array.map([](T const& value)
                    {
                        return AnySignal<T>(constant(value));
                    }));
    }

    // The item's value, deduplicated so that a fold over it advances only when
    // this element moved. A pick reports a change whenever any element of its
    // source changes, so without this every element's fold would follow every
    // other element's.
    AnySignal<int> itemValue(AnySignal<Item> item)
    {
        return item.map([](Item const& i)
                {
                    return i.value;
                })
            .check();
    }

    template <typename TFunc>
    void expectThrowsContaining(TFunc&& func, std::string const& text)
    {
        try
        {
            func();
        }
        catch (std::runtime_error const& e)
        {
            EXPECT_NE(std::string::npos, std::string(e.what()).find(text))
                << e.what();
            return;
        }

        ADD_FAILURE() << "expected a std::runtime_error containing " << text;
    }
} // namespace

TEST(arraySignal, bracedConstructionNestsAndFlattens)
{
    ArraySignal<int> array = { 1, 2, { 3, { 4, 5 } } };

    auto c = makeSignalContext(values(array));

    EXPECT_EQ((std::vector<int>{ 1, 2, 3, 4, 5 }), c.evaluate<0>().get<0>());
}

TEST(arraySignal, emptyAndSingletonBracesMeanWhatTheySay)
{
    ArraySignal<int> empty = {};
    ArraySignal<int> singleton = { 7 };
    ArraySignal<int> item = 7;

    auto c = makeSignalContext(values(empty), values(singleton),
            values(item));

    EXPECT_EQ(std::vector<int>(), c.evaluate<0>().get<0>());
    EXPECT_EQ(std::vector<int>{ 7 }, c.evaluate<1>().get<0>());
    EXPECT_EQ(std::vector<int>{ 7 }, c.evaluate<2>().get<0>());
}

TEST(arraySignal, concatJoinsArraysInOrder)
{
    ArraySignal<int> first = { 1, 2 };
    ArraySignal<int> second = { 3 };

    auto c = makeSignalContext(values(concat(first, second,
                    ArraySignal<int>(4))));

    EXPECT_EQ((std::vector<int>{ 1, 2, 3, 4 }), c.evaluate<0>().get<0>());
}

// Every operator that builds an array mints its own identities, so two arrays
// derived from one source can be concatenated. Without that, the branch a
// container makes and then rejoins would collide with itself.
TEST(arraySignal, arraysDerivedFromOneSourceConcatenate)
{
    ArraySignal<int> array = { 1, 2 };

    auto doubled = array.map([](int value)
        {
            return 2 * value;
        });

    auto tripled = array.map([](int value)
        {
            return 3 * value;
        });

    auto c = makeSignalContext(values(concat(doubled, tripled)));

    EXPECT_EQ((std::vector<int>{ 2, 4, 3, 6 }), c.evaluate<0>().get<0>());
}

// Every constructor is constant, so a constructed element's value never
// changes and nothing built from it is ever built twice.
TEST(arraySignal, aConstructedElementIsBuiltOnce)
{
    auto builds = std::make_shared<int>(0);

    ArraySignal<int> array = { 1, 2 };

    auto doubled = array.map([builds](int value)
        {
            ++*builds;
            return 2 * value;
        });

    auto c = makeSignalContext(values(doubled), values(doubled));

    EXPECT_EQ((std::vector<int>{ 2, 4 }), c.evaluate<0>().get<0>());
    EXPECT_EQ((std::vector<int>{ 2, 4 }), c.evaluate<1>().get<0>());
    EXPECT_EQ(2, *builds);

    c.update(FrameInfo(1, {}));

    EXPECT_EQ((std::vector<int>{ 2, 4 }), c.evaluate<0>().get<0>());
    EXPECT_EQ(2, *builds);
}

// An evicted value is destroyed rather than retained against the key coming
// back. Counting builds cannot see the difference; holding a weak reference to
// what was built can.
TEST(arraySignal, anEvictedValueIsDestroyed)
{
    auto input = makeInput(items({ { "a", 1 }, { "b", 2 } }));
    auto built = std::make_shared<std::vector<std::weak_ptr<int>>>();

    auto c = makeSignalContext(join(forEach(
                    AnySignal<std::vector<Item>>(input.signal),
                    itemKey,
                    [built](AnySignal<Item> item)
                    {
                        // Lives exactly as long as what the delegate returns.
                        auto marker = std::make_shared<int>(0);
                        built->push_back(marker);

                        return AnySignal<int>(item.map(
                                    [marker](Item const& i)
                                    {
                                        return i.value;
                                    }));
                    })));

    ASSERT_EQ(2u, built->size());
    EXPECT_FALSE((*built)[0].expired());
    EXPECT_FALSE((*built)[1].expired());

    input.handle.set(items({ { "b", 2 } }));
    c.update(FrameInfo(1, {}));

    EXPECT_EQ(std::vector<int>{ 2 }, c.evaluate<0>().get<0>());
    EXPECT_TRUE((*built)[0].expired());
    EXPECT_FALSE((*built)[1].expired());
}

// A duplicate that appears only after the array has been running fails the
// same way as one that was there from the start.
TEST(arraySignal, forEachThrowsOnADuplicateKeyIntroducedByAnUpdate)
{
    auto input = makeInput(items({ { "a", 1 } }));

    auto c = makeSignalContext(join(forEach(
                    AnySignal<std::vector<Item>>(input.signal),
                    itemKey,
                    [](AnySignal<Item> item)
                    {
                        return itemValue(std::move(item));
                    })));

    EXPECT_EQ(std::vector<int>{ 1 }, c.evaluate<0>().get<0>());

    input.handle.set(items({ { "a", 1 }, { "a", 2 } }));

    expectThrowsContaining([&] { c.update(FrameInfo(1, {})); },
            "duplicate key");
}

TEST(arraySignal, forEachBuildsOncePerKeyAndFollowsMembership)
{
    auto input = makeInput(items({ { "a", 1 }, { "b", 2 } }));
    auto builds = std::make_shared<int>(0);

    auto c = makeSignalContext(join(forEach(
                    AnySignal<std::vector<Item>>(input.signal),
                    itemKey,
                    [builds](AnySignal<Item> item)
                    {
                        ++*builds;
                        return itemValue(std::move(item));
                    })));

    EXPECT_EQ((std::vector<int>{ 1, 2 }), c.evaluate<0>().get<0>());
    EXPECT_EQ(2, *builds);

    // A value change reaches the built signal without rebuilding anything.
    input.handle.set(items({ { "a", 10 }, { "b", 2 } }));
    c.update(FrameInfo(1, {}));
    EXPECT_EQ((std::vector<int>{ 10, 2 }), c.evaluate<0>().get<0>());
    EXPECT_EQ(2, *builds);

    // A reorder keeps both identities and reports the new order.
    input.handle.set(items({ { "b", 2 }, { "a", 10 } }));
    c.update(FrameInfo(2, {}));
    EXPECT_EQ((std::vector<int>{ 2, 10 }), c.evaluate<0>().get<0>());
    EXPECT_EQ(2, *builds);

    // Only the new key is built.
    input.handle.set(items({ { "b", 2 }, { "c", 3 }, { "a", 10 } }));
    c.update(FrameInfo(3, {}));
    EXPECT_EQ((std::vector<int>{ 2, 3, 10 }), c.evaluate<0>().get<0>());
    EXPECT_EQ(3, *builds);

    input.handle.set(items({ { "c", 3 } }));
    c.update(FrameInfo(4, {}));
    EXPECT_EQ(std::vector<int>{ 3 }, c.evaluate<0>().get<0>());
    EXPECT_EQ(3, *builds);
}

// A key that leaves is retired, so the same key coming back is a new item
// rather than a resumption of the old one.
TEST(arraySignal, aReturningKeyIsANewIdentity)
{
    auto input = makeInput(items({ { "a", 1 } }));
    auto builds = std::make_shared<int>(0);

    auto c = makeSignalContext(join(forEach(
                    AnySignal<std::vector<Item>>(input.signal),
                    itemKey,
                    [builds](AnySignal<Item> item)
                    {
                        ++*builds;
                        return itemValue(std::move(item));
                    })));

    EXPECT_EQ(1, *builds);

    input.handle.set(items({}));
    c.update(FrameInfo(1, {}));
    EXPECT_EQ(std::vector<int>(), c.evaluate<0>().get<0>());

    input.handle.set(items({ { "a", 2 } }));
    c.update(FrameInfo(2, {}));
    EXPECT_EQ(std::vector<int>{ 2 }, c.evaluate<0>().get<0>());
    EXPECT_EQ(2, *builds);
}

// Removing the last element and adding one back is the sequence in which a
// departed key's pick has to be dropped rather than asked for a stale value.
TEST(arraySignal, shrinksAndGrowsAgain)
{
    auto input = makeInput(items({ { "a", 1 }, { "b", 2 } }));
    auto builds = std::make_shared<int>(0);

    auto c = makeSignalContext(join(forEach(
                    AnySignal<std::vector<Item>>(input.signal),
                    itemKey,
                    [builds](AnySignal<Item> item)
                    {
                        ++*builds;
                        return itemValue(std::move(item));
                    })));

    EXPECT_EQ((std::vector<int>{ 1, 2 }), c.evaluate<0>().get<0>());
    EXPECT_EQ(2, *builds);

    input.handle.set(items({ { "a", 1 } }));
    c.update(FrameInfo(1, {}));
    EXPECT_EQ(std::vector<int>{ 1 }, c.evaluate<0>().get<0>());
    EXPECT_EQ(2, *builds);

    input.handle.set(items({ { "a", 1 }, { "c", 3 } }));
    c.update(FrameInfo(2, {}));
    EXPECT_EQ((std::vector<int>{ 1, 3 }), c.evaluate<0>().get<0>());
    EXPECT_EQ(3, *builds);
}

// A neighbour arriving or leaving must not disturb what a surviving element
// has accumulated. The fold below is the sharp end of it: it is state inside
// the built value, and it would silently restart from the current value if the
// element's signal were rebuilt rather than kept.
TEST(arraySignal, aSurvivingElementKeepsItsStateThroughItsNeighbours)
{
    auto input = makeInput(items({ { "a", 1 }, { "b", 10 } }));

    auto sums = join(forEach(
                AnySignal<std::vector<Item>>(input.signal),
                itemKey,
                [](AnySignal<Item> item)
                {
                    return AnySignal<int>(itemValue(std::move(item))
                            .withPrevious([](int sum, int value)
                                {
                                    return sum + value;
                                },
                                0));
                }));

    auto c = makeSignalContext(sums);

    EXPECT_EQ((std::vector<int>{ 1, 10 }), c.evaluate<0>().get<0>());

    input.handle.set(items({ { "a", 1 }, { "b", 5 } }));
    c.update(FrameInfo(1, {}));
    EXPECT_EQ((std::vector<int>{ 1, 15 }), c.evaluate<0>().get<0>());

    // An unrelated insertion, ahead of b so that its position moves too.
    input.handle.set(items({ { "c", 100 }, { "a", 1 }, { "b", 5 } }));
    c.update(FrameInfo(2, {}));
    EXPECT_EQ((std::vector<int>{ 100, 1, 15 }), c.evaluate<0>().get<0>());

    // An unrelated removal.
    input.handle.set(items({ { "c", 100 }, { "b", 5 } }));
    c.update(FrameInfo(3, {}));
    EXPECT_EQ((std::vector<int>{ 100, 15 }), c.evaluate<0>().get<0>());

    // And b's own fold is still running from where it was.
    input.handle.set(items({ { "c", 100 }, { "b", 1 } }));
    c.update(FrameInfo(4, {}));
    EXPECT_EQ((std::vector<int>{ 100, 16 }), c.evaluate<0>().get<0>());
}

// Two contexts over one description share nothing: they build their own values
// and their membership can be driven apart.
TEST(arraySignal, twoContextsAreIndependent)
{
    auto input = makeInput(items({ { "a", 1 } }));
    auto builds = std::make_shared<int>(0);

    auto description = join(forEach(
                AnySignal<std::vector<Item>>(input.signal),
                itemKey,
                [builds](AnySignal<Item> item)
                {
                    ++*builds;
                    return itemValue(std::move(item));
                }));

    auto first = makeSignalContext(description);
    auto second = makeSignalContext(description);

    // Once per identity per context, not once per identity.
    EXPECT_EQ(2, *builds);
    EXPECT_EQ(std::vector<int>{ 1 }, first.evaluate<0>().get<0>());
    EXPECT_EQ(std::vector<int>{ 1 }, second.evaluate<0>().get<0>());

    // Only the first context sees the new member.
    input.handle.set(items({ { "a", 1 }, { "b", 2 } }));
    first.update(FrameInfo(1, {}));
    EXPECT_EQ(3, *builds);
    EXPECT_EQ((std::vector<int>{ 1, 2 }), first.evaluate<0>().get<0>());
    EXPECT_EQ(std::vector<int>{ 1 }, second.evaluate<0>().get<0>());

    // And only the second sees it leave again, while the first stays where it
    // was left.
    input.handle.set(items({ { "a", 3 } }));
    second.update(FrameInfo(1, {}));
    EXPECT_EQ(std::vector<int>{ 3 }, second.evaluate<0>().get<0>());
    EXPECT_EQ((std::vector<int>{ 1, 2 }), first.evaluate<0>().get<0>());
    EXPECT_EQ(3, *builds);
}

// Two consumers of one array within one context share its built values, which
// is what lets a container branch without duplicating what identity exists to
// protect.
TEST(arraySignal, twoConsumersInOneContextShareTheBuiltValues)
{
    auto input = makeInput(items({ { "a", 1 }, { "b", 2 } }));
    auto builds = std::make_shared<int>(0);

    auto array = forEach(
            AnySignal<std::vector<Item>>(input.signal),
            itemKey,
            [builds](AnySignal<Item> item)
            {
                ++*builds;
                return itemValue(std::move(item));
            });

    auto c = makeSignalContext(join(array), join(array));

    EXPECT_EQ(2, *builds);
    EXPECT_EQ((std::vector<int>{ 1, 2 }), c.evaluate<0>().get<0>());
    EXPECT_EQ((std::vector<int>{ 1, 2 }), c.evaluate<1>().get<0>());

    input.handle.set(items({ { "a", 1 }, { "b", 2 }, { "c", 3 } }));
    c.update(FrameInfo(1, {}));

    EXPECT_EQ(3, *builds);
    EXPECT_EQ((std::vector<int>{ 1, 2, 3 }), c.evaluate<0>().get<0>());
    EXPECT_EQ((std::vector<int>{ 1, 2, 3 }), c.evaluate<1>().get<0>());
}

// map runs when an identity appears, not when its value changes.
TEST(arraySignal, mapRunsOncePerIdentity)
{
    auto input = makeInput(items({ { "a", 1 } }));
    auto maps = std::make_shared<int>(0);

    auto array = forEach(
            AnySignal<std::vector<Item>>(input.signal),
            itemKey,
            [](AnySignal<Item> item)
            {
                return itemValue(std::move(item));
            });

    auto c = makeSignalContext(join(array.map([maps](AnySignal<int> const& sig)
                    {
                        ++*maps;
                        return AnySignal<int>(sig.map([](int value)
                                    {
                                        return 10 * value;
                                    }));
                    })));

    EXPECT_EQ(std::vector<int>{ 10 }, c.evaluate<0>().get<0>());
    EXPECT_EQ(1, *maps);

    input.handle.set(items({ { "a", 2 } }));
    c.update(FrameInfo(1, {}));
    EXPECT_EQ(std::vector<int>{ 20 }, c.evaluate<0>().get<0>());
    EXPECT_EQ(1, *maps);

    input.handle.set(items({ { "a", 2 }, { "b", 3 } }));
    c.update(FrameInfo(2, {}));
    EXPECT_EQ((std::vector<int>{ 20, 30 }), c.evaluate<0>().get<0>());
    EXPECT_EQ(2, *maps);
}

// A duplicate key almost always means the caller keyed on the wrong thing, so
// it fails rather than silently dropping or duplicating an element.
TEST(arraySignal, forEachThrowsOnADuplicateKey)
{
    auto input = makeInput(items({ { "a", 1 }, { "a", 2 } }));

    auto description = join(forEach(
                AnySignal<std::vector<Item>>(input.signal),
                itemKey,
                [](AnySignal<Item> item)
                {
                    return itemValue(std::move(item));
                }));

    expectThrowsContaining([&] { makeSignalContext(description); },
            "duplicate key");
}

TEST(arraySignal, forEachTakesAPlainVector)
{
    auto c = makeSignalContext(join(forEach(
                    items({ { "a", 1 }, { "b", 2 } }),
                    itemKey,
                    [](AnySignal<Item> item)
                    {
                        return itemValue(std::move(item));
                    })));

    EXPECT_EQ((std::vector<int>{ 1, 2 }), c.evaluate<0>().get<0>());
}
