#include <bq/signal/arraysignal.h>

#include <bq/signal/constant.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/input.h>
#include <bq/signal/merge.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>
#include <bq/signal/signaltraits.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace bq::signal;

static_assert(checkSignal<detail::ArrayJoin<int>>());

static_assert(checkSignal<detail::ArrayOnce<
        int,
        std::function<int(int const&)>,
        std::function<int(int const&, int const&)>
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

    // The array a keyed source builds, whose elements carry the item's value.
    auto keyedItems = [](AnySignal<std::vector<Item>> source)
    {
        return forEach(std::move(source), itemKey,
                [](AnySignal<Item> item)
                {
                    return itemValue(std::move(item));
                });
    };

    // Both halves of what a scattered element was handed, so that a wrong
    // slice reads as a wrong number rather than as a missing one.
    AnySignal<int> valueWithSlice(AnySignal<int> const& value,
            AnySignal<int> slice)
    {
        return merge(value, std::move(slice)).map([](int v, int s)
                {
                    return 100 * v + s;
                });
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

    using KeyedDelegate = std::function<AnySignal<int>(std::string,
            AnySignal<Item>)>;
    using KeyedByRefDelegate = std::function<AnySignal<int>(
            std::string const&, AnySignal<Item>)>;
    using KeyedByMutableRefDelegate = std::function<AnySignal<int>(
            std::string&, AnySignal<Item>)>;
    using PlainDelegate = std::function<AnySignal<int>(AnySignal<Item>)>;
    using WrongDelegate = std::function<AnySignal<int>(int, int)>;
} // namespace

// The two delegate shapes forEach accepts, and the condition its static_assert
// reports on. A delegate matching neither cannot be given a test that compiles,
// so what is pinned here is what that assert is written against.
static_assert(detail::isKeyedForEachDelegate<KeyedDelegate, std::string, Item>);
static_assert(!detail::isPlainForEachDelegate<KeyedDelegate, Item>);
static_assert(detail::isPlainForEachDelegate<PlainDelegate, Item>);
static_assert(!detail::isKeyedForEachDelegate<PlainDelegate, std::string,
        Item>);
static_assert(!detail::isKeyedForEachDelegate<WrongDelegate, std::string,
        Item>);
static_assert(!detail::isPlainForEachDelegate<WrongDelegate, Item>);

// The key is handed over as a value, so a delegate may read it however a
// parameter can be read — except by non-const reference, which is what asking
// to write back to the node's table would look like.
static_assert(detail::isKeyedForEachDelegate<KeyedByRefDelegate, std::string,
        Item>);
static_assert(!detail::isKeyedForEachDelegate<KeyedByMutableRefDelegate,
        std::string, Item>);

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
                    input.signal,
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

// An element that arrives during an update is initialized against that frame,
// so it must not also be updated in it. A fold makes the difference visible:
// advanced twice, an arrival's first value would be counted twice.
TEST(arraySignal, anArrivingElementIsAdvancedOnce)
{
    auto input = makeInput(items({ { "a", 1 } }));

    // Deliberately without the check() that itemValue() adds: it would hide a
    // repeated advance rather than report it.
    auto sums = join(forEach(
                input.signal,
                itemKey,
                [](AnySignal<Item> item)
                {
                    return AnySignal<int>(item
                            .map([](Item const& i)
                                {
                                    return i.value;
                                })
                            .withPrevious([](int sum, int value)
                                {
                                    return sum + value;
                                },
                                0));
                }));

    auto c = makeSignalContext(sums);

    EXPECT_EQ(std::vector<int>{ 1 }, c.evaluate<0>().get<0>());

    input.handle.set(items({ { "a", 1 }, { "b", 5 } }));
    c.update(FrameInfo(1, {}));

    // b folded its first value exactly once, as a did at construction; were it
    // advanced twice it would read 10. a reads 2 because a pick reports a
    // change whenever any element of its source changes, so a's fold takes its
    // unchanged value again — which is why itemValue() deduplicates and why
    // this test does not.
    EXPECT_EQ((std::vector<int>{ 2, 5 }), c.evaluate<0>().get<0>());
}

// Concatenating an array with itself makes one identity appear twice. The exit
// reports it rather than quietly giving the two occurrences one set of state
// and advancing it twice a frame.
TEST(arraySignal, joinRejectsAnArrayConcatenatedWithItself)
{
    ArraySignal<AnySignal<int>> array = AnySignal<int>(constant(1));

    expectThrowsContaining(
            [&]
            {
                makeSignalContext(join(concat(array, array)));
            },
            "appears twice");
}

// A duplicate that appears only after the array has been running fails the
// same way as one that was there from the start.
TEST(arraySignal, forEachThrowsOnADuplicateKeyIntroducedByAnUpdate)
{
    auto input = makeInput(items({ { "a", 1 } }));

    auto c = makeSignalContext(join(forEach(
                    input.signal,
                    itemKey,
                    [](AnySignal<Item> item)
                    {
                        return itemValue(std::move(item));
                    })));

    EXPECT_EQ(std::vector<int>{ 1 }, c.evaluate<0>().get<0>());

    input.handle.set(items({ { "a", 1 }, { "a", 2 } }));

    expectThrowsContaining(
            [&]
            {
                c.update(FrameInfo(1, {}));
            },
            "duplicate key");
}

TEST(arraySignal, forEachBuildsOncePerKeyAndFollowsMembership)
{
    auto input = makeInput(items({ { "a", 1 }, { "b", 2 } }));
    auto builds = std::make_shared<int>(0);

    auto c = makeSignalContext(join(forEach(
                    input.signal,
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

// The delegate is given the key its own element was minted under, and the key
// is a value rather than a signal because it cannot change. The built value
// below carries both halves, so a mispairing reads as a wrong string.
TEST(arraySignal, aKeyedDelegateIsGivenItsOwnKey)
{
    auto input = makeInput(items({ { "a", 1 }, { "b", 2 } }));
    auto builds = std::make_shared<int>(0);

    auto c = makeSignalContext(join(forEach(
                    input.signal,
                    itemKey,
                    [builds](std::string key, AnySignal<Item> item)
                    {
                        ++*builds;

                        return AnySignal<std::string>(std::move(item).map(
                                [key](Item const& i)
                                {
                                    return key + std::to_string(i.value);
                                }));
                    })));

    EXPECT_EQ((std::vector<std::string>{ "a1", "b2" }),
            c.evaluate<0>().get<0>());
    EXPECT_EQ(2, *builds);

    // The key is invariant for the identity, so it still names its element
    // after the value changed and after the membership moved around it.
    input.handle.set(items({ { "b", 20 }, { "c", 3 }, { "a", 10 } }));
    c.update(FrameInfo(1, {}));

    EXPECT_EQ((std::vector<std::string>{ "b20", "c3", "a10" }),
            c.evaluate<0>().get<0>());
    EXPECT_EQ(3, *builds);
}

// The key is a value and not a reference into the node's own table, so a
// delegate that keeps it keeps a copy and cannot outlive what it names.
TEST(arraySignal, theKeyIsGivenAsAValue)
{
    auto c = makeSignalContext(join(forEach(
                    items({ { "a", 1 } }),
                    itemKey,
                    [](auto&& key, AnySignal<Item> item)
                    {
                        static_assert(std::is_same_v<decltype(key),
                                std::string&&>);

                        return itemValue(std::move(item));
                    })));

    EXPECT_EQ(std::vector<int>{ 1 }, c.evaluate<0>().get<0>());
}

// A delegate that accepts both forms is given the key. It is strictly more to
// work with, and one that names no parameter for it cannot tell the difference.
TEST(arraySignal, aGenericDelegateIsGivenTheKeyedForm)
{
    auto arity = std::make_shared<std::size_t>(0);

    auto c = makeSignalContext(join(forEach(
                    items({ { "a", 1 } }),
                    itemKey,
                    [arity](auto&&... ts)
                    {
                        *arity = sizeof...(ts);

                        return AnySignal<int>(constant(0));
                    })));

    EXPECT_EQ(std::vector<int>{ 0 }, c.evaluate<0>().get<0>());
    EXPECT_EQ(2u, *arity);
}

// The unkeyed form is unchanged, and the two build the same array.
TEST(arraySignal, bothDelegateFormsBuildTheSameArray)
{
    auto input = makeInput(items({ { "a", 1 }, { "b", 2 } }));

    auto plain = join(forEach(input.signal, itemKey,
                [](AnySignal<Item> item)
                {
                    return itemValue(std::move(item));
                }));

    auto keyed = join(forEach(input.signal, itemKey,
                [](std::string, AnySignal<Item> item)
                {
                    return itemValue(std::move(item));
                }));

    auto c = makeSignalContext(std::move(plain), std::move(keyed));

    EXPECT_EQ((std::vector<int>{ 1, 2 }), c.evaluate<0>().get<0>());
    EXPECT_EQ((std::vector<int>{ 1, 2 }), c.evaluate<1>().get<0>());

    input.handle.set(items({ { "b", 20 } }));
    c.update(FrameInfo(1, {}));

    EXPECT_EQ(std::vector<int>{ 20 }, c.evaluate<0>().get<0>());
    EXPECT_EQ(std::vector<int>{ 20 }, c.evaluate<1>().get<0>());
}

// A key that leaves is retired, so the same key coming back is a new item
// rather than a resumption of the old one.
TEST(arraySignal, aReturningKeyIsANewIdentity)
{
    auto input = makeInput(items({ { "a", 1 } }));
    auto builds = std::make_shared<int>(0);

    auto c = makeSignalContext(join(forEach(
                    input.signal,
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
                    input.signal,
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
                input.signal,
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
                input.signal,
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
            input.signal,
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
            input.signal,
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
                input.signal,
                itemKey,
                [](AnySignal<Item> item)
                {
                    return itemValue(std::move(item));
                }));

    expectThrowsContaining(
            [&]
            {
                makeSignalContext(description);
            },
            "duplicate key");
}

// The source is any signal of a vector, not only a type-erased one. Every other
// test here passes the concrete signal makeInput() returns; this one pins the
// erased form, which binds by deduction to the Signal base AnySignal derives
// from and is subtle enough to be worth compiling on purpose.
TEST(arraySignal, forEachTakesAnErasedSignal)
{
    auto input = makeInput(items({ { "a", 1 }, { "b", 2 } }));
    AnySignal<std::vector<Item>> erased = input.signal;

    auto c = makeSignalContext(join(forEach(erased, itemKey,
                    [](AnySignal<Item> item)
                    {
                        return itemValue(std::move(item));
                    })));

    EXPECT_EQ((std::vector<int>{ 1, 2 }), c.evaluate<0>().get<0>());

    input.handle.set(items({ { "b", 20 } }));
    c.update(FrameInfo(1, {}));
    EXPECT_EQ(std::vector<int>{ 20 }, c.evaluate<0>().get<0>());
}

// A signal that is neither an input nor erased: the source only has to carry a
// std::vector<T>.
TEST(arraySignal, forEachTakesADerivedSignal)
{
    auto input = makeInput(2);

    auto source = input.signal.map([](int count)
        {
            std::vector<Item> result;
            for (int i = 0; i < count; ++i)
                result.push_back({ std::to_string(i), i });

            return result;
        });

    auto c = makeSignalContext(join(forEach(source, itemKey,
                    [](AnySignal<Item> item)
                    {
                        return itemValue(std::move(item));
                    })));

    EXPECT_EQ((std::vector<int>{ 0, 1 }), c.evaluate<0>().get<0>());

    input.handle.set(3);
    c.update(FrameInfo(1, {}));
    EXPECT_EQ((std::vector<int>{ 0, 1, 2 }), c.evaluate<0>().get<0>());
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

TEST(arraySignal, scatterGivesEachElementItsOwnSlice)
{
    ArraySignal<int> array = { 1, 2, 3 };

    auto scattered = scatter(array,
            constant(std::vector<int>{ 10, 20, 30 }),
            [](int value, AnySignal<int> slice)
            {
                return AnySignal<int>(slice.map([value](int s)
                            {
                                return 100 * value + s;
                            }));
            });

    auto c = makeSignalContext(join(scattered));

    EXPECT_EQ((std::vector<int>{ 110, 220, 330 }),
            c.evaluate<0>().get<0>());
}

// The construction scatter exists for: fan the elements in, compute over all
// of them, and hand each one its share back. A positional aggregate makes an
// element's own slice the slice of wherever it now is, so what this pins is
// that a reorder recomputes it: were the map kept from the previous update,
// a would still read the 1 it was given at position 0. The identity keying is
// what the elementwise aggregate below tests.
TEST(arraySignal, scatterFollowsMembershipThroughAReorder)
{
    auto input = makeInput(items({ { "a", 1 }, { "b", 2 } }));

    auto array = keyedItems(input.signal);

    auto aggregate = join(array).map([](std::vector<int> const& values)
        {
            std::vector<int> result;
            for (std::size_t i = 0; i != values.size(); ++i)
                result.push_back(10 * static_cast<int>(i) + values[i]);

            return result;
        });

    auto c = makeSignalContext(join(scatter(array, aggregate,
                    &valueWithSlice)));

    EXPECT_EQ((std::vector<int>{ 101, 212 }), c.evaluate<0>().get<0>());

    // Both identities survive, and each one's slice is the one computed for
    // where it is now: a is at position 1 and reads 11, not the 1 it read at
    // position 0.
    input.handle.set(items({ { "b", 2 }, { "a", 1 } }));
    c.update(FrameInfo(1, {}));

    EXPECT_EQ((std::vector<int>{ 202, 111 }), c.evaluate<0>().get<0>());
}

// The property the design exists for, at the fan-out: a neighbour arriving and
// leaving must not disturb what a survivor has accumulated from its slices.
// The aggregate here is elementwise, so a survivor's slice is stable while its
// position is not.
TEST(arraySignal, aScatteredSurvivorKeepsItsSliceAndItsState)
{
    auto input = makeInput(items({ { "a", 1 }, { "b", 10 } }));

    auto array = keyedItems(input.signal);

    auto aggregate = join(array).map([](std::vector<int> const& values)
        {
            std::vector<int> result;
            for (int value : values)
                result.push_back(10 * value);

            return result;
        });

    auto sums = join(scatter(array, aggregate,
                [](AnySignal<int> const&, AnySignal<int> slice)
                {
                    return AnySignal<int>(std::move(slice)
                            .check()
                            .withPrevious([](int sum, int value)
                                {
                                    return sum + value;
                                },
                                0));
                }));

    auto c = makeSignalContext(sums);

    EXPECT_EQ((std::vector<int>{ 10, 100 }), c.evaluate<0>().get<0>());

    input.handle.set(items({ { "a", 1 }, { "b", 5 } }));
    c.update(FrameInfo(1, {}));
    EXPECT_EQ((std::vector<int>{ 10, 150 }), c.evaluate<0>().get<0>());

    // An arrival ahead of both, which moves every position but no slice.
    input.handle.set(items({ { "c", 100 }, { "a", 1 }, { "b", 5 } }));
    c.update(FrameInfo(2, {}));
    EXPECT_EQ((std::vector<int>{ 1000, 10, 150 }), c.evaluate<0>().get<0>());

    // And a departure.
    input.handle.set(items({ { "c", 100 }, { "b", 5 } }));
    c.update(FrameInfo(3, {}));
    EXPECT_EQ((std::vector<int>{ 1000, 150 }), c.evaluate<0>().get<0>());

    input.handle.set(items({ { "c", 100 }, { "b", 1 } }));
    c.update(FrameInfo(4, {}));
    EXPECT_EQ((std::vector<int>{ 1000, 160 }), c.evaluate<0>().get<0>());
}

// The delegate runs when an identity appears, as map's function does: neither a
// changed item value nor a changed aggregate rebuilds anything.
TEST(arraySignal, scatterRunsItsDelegateOncePerIdentity)
{
    auto input = makeInput(items({ { "a", 1 } }));
    auto offset = makeInput(std::vector<int>{ 1000 });
    auto builds = std::make_shared<int>(0);

    auto array = keyedItems(input.signal);

    auto c = makeSignalContext(join(scatter(array, offset.signal,
                    [builds](AnySignal<int> const& value,
                        AnySignal<int> slice)
                    {
                        ++*builds;
                        return valueWithSlice(value, std::move(slice));
                    })));

    EXPECT_EQ((std::vector<int>{ 1100 }), c.evaluate<0>().get<0>());
    EXPECT_EQ(1, *builds);

    input.handle.set(items({ { "a", 2 } }));
    c.update(FrameInfo(1, {}));
    EXPECT_EQ((std::vector<int>{ 1200 }), c.evaluate<0>().get<0>());
    EXPECT_EQ(1, *builds);

    offset.handle.set(std::vector<int>{ 2000 });
    c.update(FrameInfo(2, {}));
    EXPECT_EQ((std::vector<int>{ 2200 }), c.evaluate<0>().get<0>());
    EXPECT_EQ(1, *builds);

    input.handle.set(items({ { "a", 2 }, { "b", 3 } }));
    offset.handle.set(std::vector<int>{ 2000, 3000 });
    c.update(FrameInfo(3, {}));
    EXPECT_EQ((std::vector<int>{ 2200, 3300 }), c.evaluate<0>().get<0>());
    EXPECT_EQ(2, *builds);
}

// Emptying the array releases the last pick on the slice signal, whose
// per-context data may then expire. A later arrival has to find or rebuild it
// rather than fail, which is the same sequence forEach's shrink and grow
// covers one level down.
TEST(arraySignal, scatterShrinksToEmptyAndGrowsAgain)
{
    auto input = makeInput(items({ { "a", 1 } }));

    auto array = keyedItems(input.signal);

    auto aggregate = join(array).map([](std::vector<int> const& values)
        {
            std::vector<int> result;
            for (int value : values)
                result.push_back(10 * value);

            return result;
        });

    auto c = makeSignalContext(join(scatter(array, aggregate,
                    &valueWithSlice)));

    EXPECT_EQ((std::vector<int>{ 110 }), c.evaluate<0>().get<0>());

    input.handle.set(items({}));
    c.update(FrameInfo(1, {}));
    EXPECT_EQ(std::vector<int>(), c.evaluate<0>().get<0>());

    input.handle.set(items({ { "b", 2 } }));
    c.update(FrameInfo(2, {}));
    EXPECT_EQ((std::vector<int>{ 220 }), c.evaluate<0>().get<0>());
}

// scatter() itself only describes the graph, so the mismatch surfaces from
// whatever reads a slice — here, realizing the description.
TEST(arraySignal, scatterRejectsAnAggregateOfTheWrongSize)
{
    ArraySignal<int> array = { 1, 2 };

    auto scattered = scatter(array, constant(std::vector<int>{ 10 }),
            [](int, AnySignal<int> slice)
            {
                return slice;
            });

    expectThrowsContaining(
            [&]
            {
                makeSignalContext(join(scattered));
            },
            "aggregate size 1 does not match membership size 2");
}

// A mismatch that appears only after the array has been running fails the same
// way as one that was there from the start: the membership grew away from the
// aggregate, and the update that observes it reports it.
TEST(arraySignal, scatterRejectsAMembershipThatGrewPastItsAggregate)
{
    auto input = makeInput(items({ { "a", 1 } }));

    auto c = makeSignalContext(join(scatter(keyedItems(input.signal),
                    constant(std::vector<int>{ 10 }), &valueWithSlice)));

    EXPECT_EQ((std::vector<int>{ 110 }), c.evaluate<0>().get<0>());

    input.handle.set(items({ { "a", 1 }, { "b", 2 } }));

    expectThrowsContaining(
            [&]
            {
                c.update(FrameInfo(1, {}));
            },
            "aggregate size 1 does not match membership size 2");
}

// The other direction, which is the one a container reaches when a child is
// removed: every surviving element is still there to read a slice, and the
// aggregate now has one too many.
TEST(arraySignal, scatterRejectsAMembershipThatShrankBelowItsAggregate)
{
    auto input = makeInput(items({ { "a", 1 }, { "b", 2 } }));

    auto c = makeSignalContext(join(scatter(keyedItems(input.signal),
                    constant(std::vector<int>{ 10, 20 }), &valueWithSlice)));

    EXPECT_EQ((std::vector<int>{ 110, 220 }), c.evaluate<0>().get<0>());

    input.handle.set(items({ { "a", 1 } }));

    expectThrowsContaining(
            [&]
            {
                c.update(FrameInfo(1, {}));
            },
            "aggregate size 2 does not match membership size 1");
}

// Two consumers of one scattered array in one context share its built values,
// as two consumers of any array do.
TEST(arraySignal, twoConsumersOfOneScatterShareItsBuiltValues)
{
    auto input = makeInput(items({ { "a", 1 } }));
    auto builds = std::make_shared<int>(0);

    auto array = keyedItems(input.signal);

    auto aggregate = join(array).map([](std::vector<int> const& values)
        {
            std::vector<int> result;
            for (int value : values)
                result.push_back(10 * value);

            return result;
        });

    auto scattered = scatter(array, aggregate,
            [builds](AnySignal<int> const& value, AnySignal<int> slice)
            {
                ++*builds;
                return valueWithSlice(value, std::move(slice));
            });

    auto c = makeSignalContext(join(scattered), join(scattered));

    EXPECT_EQ(1, *builds);
    EXPECT_EQ((std::vector<int>{ 110 }), c.evaluate<0>().get<0>());
    EXPECT_EQ((std::vector<int>{ 110 }), c.evaluate<1>().get<0>());

    input.handle.set(items({ { "a", 1 }, { "b", 2 } }));
    c.update(FrameInfo(1, {}));

    EXPECT_EQ(2, *builds);
    EXPECT_EQ((std::vector<int>{ 110, 220 }), c.evaluate<0>().get<0>());
    EXPECT_EQ((std::vector<int>{ 110, 220 }), c.evaluate<1>().get<0>());
}

// A self-concatenated array gives one identity two slices. The node keyed by
// identity is what reports it, before any slice is read.
TEST(arraySignal, scatterRejectsAnArrayConcatenatedWithItself)
{
    ArraySignal<int> array = { 1 };

    expectThrowsContaining(
            [&]
            {
                makeSignalContext(join(scatter(concat(array, array),
                                constant(std::vector<int>{ 10, 20 }),
                                [](int, AnySignal<int> slice)
                                {
                                    return slice;
                                })));
            },
            "duplicate key");
}

TEST(arraySignal, twoContextsOverOneScatterAreIndependent)
{
    auto input = makeInput(items({ { "a", 1 } }));
    auto builds = std::make_shared<int>(0);

    auto array = keyedItems(input.signal);

    auto aggregate = join(array).map([](std::vector<int> const& values)
        {
            std::vector<int> result;
            for (int value : values)
                result.push_back(10 * value);

            return result;
        });

    auto description = join(scatter(array, aggregate,
                [builds](AnySignal<int> const& value, AnySignal<int> slice)
                {
                    ++*builds;
                    return valueWithSlice(value, std::move(slice));
                }));

    auto first = makeSignalContext(description);
    auto second = makeSignalContext(description);

    EXPECT_EQ(2, *builds);
    EXPECT_EQ((std::vector<int>{ 110 }), first.evaluate<0>().get<0>());
    EXPECT_EQ((std::vector<int>{ 110 }), second.evaluate<0>().get<0>());

    input.handle.set(items({ { "a", 1 }, { "b", 2 } }));
    first.update(FrameInfo(1, {}));

    EXPECT_EQ(3, *builds);
    EXPECT_EQ((std::vector<int>{ 110, 220 }), first.evaluate<0>().get<0>());
    EXPECT_EQ((std::vector<int>{ 110 }), second.evaluate<0>().get<0>());
}

// The residual hazard, pinned rather than described: the aggregate is
// positional, so one of the right length in the wrong order passes the only
// check there is and every element is handed a neighbour's slice. Aligned, the
// elements below would read 110, 220 and 330.
TEST(arraySignal, scatterCannotDetectAMisorderedAggregate)
{
    ArraySignal<int> array = { 1, 2, 3 };

    auto aggregate = values(array).map([](std::vector<int> const& v)
        {
            std::vector<int> result(v.rbegin(), v.rend());
            for (int& value : result)
                value *= 10;

            return result;
        });

    auto c = makeSignalContext(join(scatter(array, aggregate,
                    [](int value, AnySignal<int> slice)
                    {
                        return AnySignal<int>(slice.map([value](int s)
                                    {
                                        return 100 * value + s;
                                    }));
                    })));

    EXPECT_EQ((std::vector<int>{ 130, 220, 310 }),
            c.evaluate<0>().get<0>());
}

// The same hazard reached the way a caller is most likely to reach it: an
// aggregate that is not derived from this array, and so still describes the
// membership of an earlier update. The count matches, so nothing is reported
// and c is handed the slice computed for b.
TEST(arraySignal, scatterCannotDetectAnAggregateFromAnEarlierMembership)
{
    auto input = makeInput(items({ { "a", 1 }, { "b", 2 } }));
    auto aggregate = makeInput(std::vector<int>{ 10, 20 });

    auto c = makeSignalContext(join(scatter(keyedItems(input.signal),
                    aggregate.signal, &valueWithSlice)));

    EXPECT_EQ((std::vector<int>{ 110, 220 }), c.evaluate<0>().get<0>());

    input.handle.set(items({ { "a", 1 }, { "c", 3 } }));
    c.update(FrameInfo(1, {}));

    EXPECT_EQ((std::vector<int>{ 110, 320 }), c.evaluate<0>().get<0>());
}
