#include <bq/signal/arraysignal.h>
#include <bq/signal/arraysignallayout.h>

#include <bq/signal/constant.h>
#include <bq/signal/input.h>
#include <bq/signal/signalcontext.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace bq::signal;
using bq::signal::layout::ArrayKey;
using bq::signal::layout::extract;
using bq::signal::layout::scatter;

namespace
{
    /** Fans an array back out to a plain signal of its element values. */
    template <typename T>
    AnySignal<std::vector<T>> valuesOf(ArraySignal<T> const& array)
    {
        return extract(array, [](AnySignal<T> value)
            {
                return value;
            }).values();
    }

    FrameInfo nextFrame(uint64_t id)
    {
        return FrameInfo(id, signal_time_t(0));
    }

    /** An item with an identity of its own and a value that can change. */
    struct Item
    {
        int key = 0;
        int value = 0;

        bool operator==(Item const& rhs) const
        {
            return key == rhs.key && value == rhs.value;
        }
    };

    std::vector<Item> items(std::vector<std::pair<int, int>> const& pairs)
    {
        std::vector<Item> result;
        for (auto const& pair : pairs)
            result.push_back(Item{ pair.first, pair.second });

        return result;
    }
} // namespace

TEST(arraySignal, constructFromBraces)
{
    ArraySignal<int> array = { 1, 2, 3 };

    auto context = makeSignalContext(valuesOf(array));

    EXPECT_EQ((std::vector<int>{ 1, 2, 3 }), context.evaluate<0>().get<0>());
}

TEST(arraySignal, constructEmpty)
{
    ArraySignal<int> array = {};

    auto context = makeSignalContext(valuesOf(array));

    EXPECT_TRUE(context.evaluate<0>().get<0>().empty());
}

TEST(arraySignal, constructFromVector)
{
    ArraySignal<int> array(std::vector<int>{ 4, 5 });

    auto context = makeSignalContext(valuesOf(array));

    EXPECT_EQ((std::vector<int>{ 4, 5 }), context.evaluate<0>().get<0>());
}

TEST(arraySignal, constructFromSignal)
{
    auto input = makeInput(std::vector<int>{ 1, 2 });
    ArraySignal<int> array(AnySignal<std::vector<int>>(input.signal));

    auto context = makeSignalContext(valuesOf(array));

    EXPECT_EQ((std::vector<int>{ 1, 2 }), context.evaluate<0>().get<0>());

    input.handle.set(std::vector<int>{ 7, 8, 9 });
    context.update(nextFrame(1));

    EXPECT_EQ((std::vector<int>{ 7, 8, 9 }), context.evaluate<0>().get<0>());
}

TEST(arraySignal, constructFromNestedBraces)
{
    ArraySignal<int> inner = { 2, 3 };
    ArraySignal<int> array = { 1, inner, 4 };

    auto context = makeSignalContext(valuesOf(array));

    EXPECT_EQ((std::vector<int>{ 1, 2, 3, 4 }), context.evaluate<0>().get<0>());
}

TEST(arraySignal, concat)
{
    ArraySignal<int> a = { 1, 2 };
    ArraySignal<int> b = { 3 };

    auto context = makeSignalContext(valuesOf(concat(a, b)));

    EXPECT_EQ((std::vector<int>{ 1, 2, 3 }), context.evaluate<0>().get<0>());
}

TEST(arraySignal, concatKeepsIdentitiesDistinct)
{
    ArraySignal<int> a = { 1 };
    ArraySignal<int> b = { 1 };

    auto keyed = extract(concat(a, b), [](AnySignal<int> value)
        {
            return value;
        });

    auto context = makeSignalContext(keyed.sig());

    auto const& ids = context.evaluate<0>().get<0>();
    ASSERT_EQ(2u, ids.size());
    EXPECT_NE(ids[0], ids[1]);
}

TEST(arraySignal, mapIsValueLevel)
{
    auto input = makeInput(std::vector<int>{ 1, 2 });
    ArraySignal<int> array(AnySignal<std::vector<int>>(input.signal));

    auto calls = std::make_shared<int>(0);
    auto mapped = array.map([calls](int value)
        {
            ++*calls;
            return value * 10;
        });

    auto context = makeSignalContext(valuesOf(mapped));

    EXPECT_EQ((std::vector<int>{ 10, 20 }), context.evaluate<0>().get<0>());

    int const before = *calls;
    input.handle.set(std::vector<int>{ 3, 4 });
    context.update(nextFrame(1));

    EXPECT_EQ((std::vector<int>{ 30, 40 }), context.evaluate<0>().get<0>());
    EXPECT_GT(*calls, before);
}

TEST(arraySignal, forEachRunsDelegateOncePerKey)
{
    auto input = makeInput(items({ { 1, 10 }, { 2, 20 } }));

    auto calls = std::make_shared<int>(0);
    auto built = forEach(AnySignal<std::vector<Item>>(input.signal),
            [](Item const& item)
            {
                return item.key;
            },
            [calls](AnySignal<Item> item)
            {
                ++*calls;
                return item.map([](Item const& i)
                    {
                        return i.value;
                    });
            });

    auto context = makeSignalContext(valuesOf(built));

    EXPECT_EQ(2, *calls);

    input.handle.set(items({ { 1, 10 }, { 2, 20 }, { 3, 30 } }));
    context.update(nextFrame(1));

    EXPECT_EQ(3, *calls);

    input.handle.set(items({ { 3, 30 }, { 1, 10 }, { 2, 20 } }));
    context.update(nextFrame(2));

    EXPECT_EQ(3, *calls);
}

TEST(arraySignal, forEachDoesNotRebuildOnValueChange)
{
    auto input = makeInput(items({ { 1, 10 }, { 2, 20 } }));

    auto calls = std::make_shared<int>(0);
    auto built = forEach(AnySignal<std::vector<Item>>(input.signal),
            [](Item const& item)
            {
                return item.key;
            },
            [calls](AnySignal<Item> item)
            {
                ++*calls;
                return item.map([](Item const& i)
                    {
                        return i.value;
                    });
            });

    auto context = makeSignalContext(
            extract(built, [](AnySignal<AnySignal<int>> value)
                {
                    return value.join();
                }).values());

    EXPECT_EQ(2, *calls);
    EXPECT_EQ((std::vector<int>{ 10, 20 }), context.evaluate<0>().get<0>());

    input.handle.set(items({ { 1, 11 }, { 2, 22 } }));
    context.update(nextFrame(1));

    EXPECT_EQ(2, *calls);
    EXPECT_EQ((std::vector<int>{ 11, 22 }), context.evaluate<0>().get<0>());
}

TEST(arraySignal, forEachRebuildsOnChangedKey)
{
    auto input = makeInput(items({ { 1, 10 } }));

    auto calls = std::make_shared<int>(0);
    auto built = forEach(AnySignal<std::vector<Item>>(input.signal),
            [](Item const& item)
            {
                return item.key;
            },
            [calls](AnySignal<Item> item)
            {
                ++*calls;
                return item.map([](Item const& i)
                    {
                        return i.value;
                    });
            });

    auto context = makeSignalContext(valuesOf(built));

    EXPECT_EQ(1, *calls);

    input.handle.set(items({ { 2, 10 } }));
    context.update(nextFrame(1));

    EXPECT_EQ(2, *calls);
}

TEST(arraySignal, forEachPreservesOrderOnReorder)
{
    auto input = makeInput(items({ { 1, 10 }, { 2, 20 }, { 3, 30 } }));

    auto built = forEach(AnySignal<std::vector<Item>>(input.signal),
            [](Item const& item)
            {
                return item.key;
            },
            [](AnySignal<Item> item)
            {
                return item.map([](Item const& i)
                    {
                        return i.value;
                    });
            });

    auto keyed = extract(built, [](AnySignal<AnySignal<int>> value)
        {
            return value.join();
        });

    auto context = makeSignalContext(keyed.sig());

    auto const idsBefore = context.evaluate<0>().get<0>();
    EXPECT_EQ((std::vector<int>{ 10, 20, 30 }), context.evaluate<0>().get<1>());

    input.handle.set(items({ { 3, 30 }, { 1, 10 }, { 2, 20 } }));
    context.update(nextFrame(1));

    auto const idsAfter = context.evaluate<0>().get<0>();
    EXPECT_EQ((std::vector<int>{ 30, 10, 20 }), context.evaluate<0>().get<1>());

    ASSERT_EQ(3u, idsAfter.size());
    EXPECT_EQ(idsBefore[2], idsAfter[0]);
    EXPECT_EQ(idsBefore[0], idsAfter[1]);
    EXPECT_EQ(idsBefore[1], idsAfter[2]);
}

TEST(arraySignal, forEachEvictsRemovedKeys)
{
    auto input = makeInput(items({ { 1, 10 }, { 2, 20 } }));

    auto alive = std::make_shared<int>(0);
    struct Tracker
    {
        explicit Tracker(std::shared_ptr<int> alive) :
            alive_(std::move(alive))
        {
            ++*alive_;
        }

        Tracker(Tracker const& rhs) :
            alive_(rhs.alive_)
        {
            ++*alive_;
        }

        ~Tracker()
        {
            --*alive_;
        }

        Tracker& operator=(Tracker const&) = delete;

        std::shared_ptr<int> alive_;
    };

    auto built = forEach(AnySignal<std::vector<Item>>(input.signal),
            [](Item const& item)
            {
                return item.key;
            },
            [alive](AnySignal<Item>)
            {
                return std::make_shared<Tracker>(alive);
            });

    auto context = makeSignalContext(valuesOf(built));

    EXPECT_EQ(2, *alive);

    input.handle.set(items({ { 1, 10 } }));
    context.update(nextFrame(1));

    EXPECT_EQ(1u, context.evaluate<0>().get<0>().size());
    EXPECT_EQ(1, *alive);
}

TEST(arraySignal, siblingStateSurvivesInsertionAndRemoval)
{
    auto input = makeInput(items({ { 1, 1 } }));

    auto built = forEach(AnySignal<std::vector<Item>>(input.signal),
            [](Item const& item)
            {
                return item.key;
            },
            [](AnySignal<Item> item)
            {
                return item
                    .map([](Item const& i)
                        {
                            return i.value;
                        })
                    .withPrevious([](int total, int value)
                        {
                            return total + value;
                        }, 0);
            });

    auto keyed = extract(built, [](AnySignal<AnySignal<int>> value)
        {
            return value.join();
        });

    auto context = makeSignalContext(keyed.values());

    ASSERT_EQ(1u, context.evaluate<0>().get<0>().size());
    int const initial = context.evaluate<0>().get<0>()[0];

    input.handle.set(items({ { 1, 1 } }));
    context.update(nextFrame(1));

    ASSERT_EQ(1u, context.evaluate<0>().get<0>().size());
    int const accumulated = context.evaluate<0>().get<0>()[0];
    EXPECT_GT(accumulated, initial);

    input.handle.set(items({ { 1, 1 }, { 2, 100 } }));
    context.update(nextFrame(2));

    ASSERT_EQ(2u, context.evaluate<0>().get<0>().size());
    EXPECT_EQ(accumulated + 1, context.evaluate<0>().get<0>()[0]);

    int const afterInsert = context.evaluate<0>().get<0>()[0];

    input.handle.set(items({ { 1, 1 } }));
    context.update(nextFrame(3));

    ASSERT_EQ(1u, context.evaluate<0>().get<0>().size());
    EXPECT_EQ(afterInsert + 1, context.evaluate<0>().get<0>()[0]);
}

TEST(arraySignal, extractAppliesFunctionOncePerIdentity)
{
    auto input = makeInput(std::vector<int>{ 1, 2 });
    ArraySignal<int> array(AnySignal<std::vector<int>>(input.signal));

    auto calls = std::make_shared<int>(0);
    auto keyed = extract(array, [calls](AnySignal<int> value)
        {
            ++*calls;
            return value.map([](int v)
                {
                    return v * 2;
                });
        });

    auto context = makeSignalContext(keyed.values());

    EXPECT_EQ(2, *calls);
    EXPECT_EQ((std::vector<int>{ 2, 4 }), context.evaluate<0>().get<0>());

    input.handle.set(std::vector<int>{ 5, 6, 7 });
    context.update(nextFrame(1));

    EXPECT_EQ(3, *calls);
    EXPECT_EQ((std::vector<int>{ 10, 12, 14 }), context.evaluate<0>().get<0>());
}

TEST(arraySignal, mapValuesSeesEveryValueAndKeepsAlignment)
{
    ArraySignal<int> array = { 1, 2, 3 };

    auto keyed = extract(array, [](AnySignal<int> value)
        {
            return value;
        });

    auto sizes = keyed.mapValues([](std::vector<int> const& values)
        {
            std::vector<int> result;
            int total = 0;
            for (int value : values)
                total += value;

            for (size_t i = 0; i < values.size(); ++i)
                result.push_back(total);

            return result;
        });

    auto context = makeSignalContext(sizes.values());

    EXPECT_EQ((std::vector<int>{ 6, 6, 6 }), context.evaluate<0>().get<0>());
}

TEST(arraySignal, mapValuesTakesAmbientExtras)
{
    ArraySignal<int> array = { 1, 2 };
    auto extra = makeInput(10);

    auto scaled = extract(array, [](AnySignal<int> value)
        {
            return value;
        })
        .mapValues([](int factor, std::vector<int> const& values)
            {
                std::vector<int> result;
                for (int value : values)
                    result.push_back(value * factor);

                return result;
            }, AnySignal<int>(extra.signal));

    auto context = makeSignalContext(scaled.values());

    EXPECT_EQ((std::vector<int>{ 10, 20 }), context.evaluate<0>().get<0>());

    extra.handle.set(3);
    context.update(nextFrame(1));

    EXPECT_EQ((std::vector<int>{ 3, 6 }), context.evaluate<0>().get<0>());
}

TEST(arraySignal, mapKeyedCarriesTheKey)
{
    ArraySignal<int> array = { 1, 2, 3 };

    auto keyed = extract(array, [](AnySignal<int> value)
        {
            return value;
        });

    auto reversed = keyed.mapKeyed(
            [](std::vector<std::pair<ArrayKey, int>> const& input)
            {
                std::vector<std::pair<ArrayKey, int>> result(input.rbegin(),
                        input.rend());
                return result;
            });

    auto context = makeSignalContext(reversed.values());

    EXPECT_EQ((std::vector<int>{ 3, 2, 1 }), context.evaluate<0>().get<0>());
}

TEST(arraySignal, extractMapValuesScatterRoundTrip)
{
    auto input = makeInput(items({ { 1, 1 }, { 2, 2 }, { 3, 3 } }));

    auto array = forEach(AnySignal<std::vector<Item>>(input.signal),
            [](Item const& item)
            {
                return item.key;
            },
            [](AnySignal<Item> item)
            {
                return item;
            });

    auto totals = extract(array, [](AnySignal<AnySignal<Item>> value)
        {
            return value.join().map([](Item const& item)
                {
                    return item.value;
                });
        })
        .mapValues([](std::vector<int> const& values)
            {
                int total = 0;
                for (int value : values)
                    total += value;

                std::vector<int> result;
                for (size_t i = 0; i < values.size(); ++i)
                    result.push_back(total);

                return result;
            });

    auto built = scatter(array, totals,
            [](AnySignal<AnySignal<Item>> item, AnySignal<int> total)
            {
                return item.join()
                    .map([](Item const& i)
                        {
                            return i.value;
                        })
                    .merge(std::move(total))
                    .map([](int value, int sum)
                        {
                            return value * 1000 + sum;
                        });
            });

    auto context = makeSignalContext(
            extract(built, [](AnySignal<AnySignal<int>> value)
                {
                    return value.join();
                }).values());

    EXPECT_EQ((std::vector<int>{ 1006, 2006, 3006 }),
            context.evaluate<0>().get<0>());

    input.handle.set(items({ { 3, 3 }, { 1, 1 }, { 2, 2 } }));
    context.update(nextFrame(1));

    EXPECT_EQ((std::vector<int>{ 3006, 1006, 2006 }),
            context.evaluate<0>().get<0>());

    input.handle.set(items({ { 3, 3 }, { 1, 1 }, { 2, 2 }, { 4, 4 } }));
    context.update(nextFrame(2));

    EXPECT_EQ((std::vector<int>{ 3010, 1010, 2010, 4010 }),
            context.evaluate<0>().get<0>());
}

TEST(arraySignal, scatterBuildsOncePerIdentity)
{
    auto input = makeInput(items({ { 1, 1 }, { 2, 2 } }));

    auto array = forEach(AnySignal<std::vector<Item>>(input.signal),
            [](Item const& item)
            {
                return item.key;
            },
            [](AnySignal<Item> item)
            {
                return item;
            });

    auto counts = extract(array, [](AnySignal<AnySignal<Item>> value)
        {
            return value.join().map([](Item const& item)
                {
                    return item.value;
                });
        });

    auto calls = std::make_shared<int>(0);
    auto built = scatter(array, counts,
            [calls](AnySignal<AnySignal<Item>>, AnySignal<int> value)
            {
                ++*calls;
                return value;
            });

    auto context = makeSignalContext(
            extract(built, [](AnySignal<AnySignal<int>> value)
                {
                    return value.join();
                }).values());

    EXPECT_EQ(2, *calls);
    EXPECT_EQ((std::vector<int>{ 1, 2 }), context.evaluate<0>().get<0>());

    input.handle.set(items({ { 1, 5 }, { 2, 6 } }));
    context.update(nextFrame(1));

    EXPECT_EQ(2, *calls);
    EXPECT_EQ((std::vector<int>{ 5, 6 }), context.evaluate<0>().get<0>());
}

TEST(arraySignal, valuesDiscardsIdentity)
{
    ArraySignal<std::string> array = { std::string("a"), std::string("b") };

    auto keyed = extract(array, [](AnySignal<std::string> value)
        {
            return value;
        });

    auto context = makeSignalContext(keyed.values());

    EXPECT_EQ((std::vector<std::string>{ "a", "b" }),
            context.evaluate<0>().get<0>());
}

TEST(arraySignal, duplicateKeysAssertInDebug)
{
    auto build = []
    {
        auto array = forEach(std::vector<Item>(items({ { 1, 1 }, { 1, 2 } })),
                [](Item const& item)
                {
                    return item.key;
                },
                [](AnySignal<Item> item)
                {
                    return item.map([](Item const& i)
                        {
                            return i.value;
                        });
                });

        auto context = makeSignalContext(valuesOf(array));
        EXPECT_EQ(2u, context.evaluate<0>().get<0>().size());
    };

    EXPECT_DEBUG_DEATH(build(), "duplicate key");
}
