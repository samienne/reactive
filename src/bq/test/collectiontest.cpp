#include <bq/signal/collection.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <future>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

using namespace bq::signal;

namespace
{
    template <typename T>
    std::vector<T> values(Collection<T> const& collection)
    {
        auto const values = collection.read().values();
        return std::vector<T>(values.begin(), values.end());
    }

    template <typename T>
    std::vector<std::uint64_t> ids(Collection<T> const& collection)
    {
        std::vector<std::uint64_t> result;
        for (auto const& item : collection.read().items())
            result.push_back(item.id);

        return result;
    }
} // namespace

TEST(collection, iterateForward)
{
    Collection<std::string> collection;
    auto transaction = collection.write();
    auto const items = transaction.items();

    transaction.pushBack("test 1");
    transaction.pushBack("test 2");
    transaction.pushBack("test 3");

    EXPECT_EQ(3u, transaction.size());

    auto i = items.begin();
    EXPECT_EQ("test 1", *i->value);
    ++i;
    EXPECT_EQ("test 2", *i->value);
    ++i;
    EXPECT_EQ("test 3", *i->value);
    ++i;
    EXPECT_EQ(items.end(), i);
}

TEST(collection, iterateBackward)
{
    Collection<std::string> collection;
    auto transaction = collection.write();
    auto const items = transaction.items();

    transaction.pushBack("test 1");
    transaction.pushBack("test 2");
    transaction.pushBack("test 3");

    auto i = items.rbegin();
    EXPECT_EQ("test 3", *i->value);
    ++i;
    EXPECT_EQ("test 2", *i->value);
    ++i;
    EXPECT_EQ("test 1", *i->value);
    ++i;
    EXPECT_EQ(items.rend(), i);
}

TEST(collection, iterateConst)
{
    Collection<std::string> col;
    {
        auto transaction = col.write();
        transaction.pushFront("test 1");
        transaction.pushFront("test 2");
        transaction.pushFront("test 3");
    }

    auto const& collection = col;
    auto view = collection.read();

    EXPECT_EQ(3u, view.size());
    auto values = view.values();
    EXPECT_EQ((std::vector<std::string>{ "test 3", "test 2", "test 1" }),
            std::vector<std::string>(values.begin(), values.end()));
    EXPECT_EQ((std::vector<std::string>{ "test 1", "test 2", "test 3" }),
            std::vector<std::string>(std::make_reverse_iterator(values.end()),
                std::make_reverse_iterator(values.begin())));
}

TEST(collection, insert)
{
    Collection<int> collection;
    {
        auto transaction = collection.write();
        auto const items = transaction.items();
        transaction.insert(items.begin(), 10);
        transaction.insert(items.end(), 20);
        transaction.insert(items.begin(), 30);
    }

    EXPECT_EQ((std::vector<int>{ 30, 10, 20 }), values(collection));
}

TEST(collection, idsAreStableAndNeverReused)
{
    Collection<std::string> collection;
    {
        auto transaction = collection.write();
        transaction.pushBack("a");
        transaction.pushBack("b");
    }

    auto const before = ids(collection);
    ASSERT_EQ(2u, before.size());
    EXPECT_NE(before[0], before[1]);

    {
        auto transaction = collection.write();
        transaction.update(transaction.items().begin(), "a2");
        transaction.eraseWithId(before[1]);
        transaction.pushBack("c");
    }

    auto const after = ids(collection);
    ASSERT_EQ(2u, after.size());
    EXPECT_EQ(before[0], after[0]);
    EXPECT_NE(before[0], after[1]);
    EXPECT_NE(before[1], after[1]);
    EXPECT_EQ((std::vector<std::string>{ "a2", "c" }), values(collection));
}

TEST(collection, updateStampsOnlyThatItem)
{
    Collection<int> collection;
    {
        auto transaction = collection.write();
        transaction.pushBack(1);
        transaction.pushBack(2);
    }

    auto const first = collection.snapshot();
    EXPECT_EQ(1u, first->generation());

    {
        auto transaction = collection.write();
        transaction.update(transaction.items().begin() + 1, 20);
    }

    auto const second = collection.snapshot();
    EXPECT_EQ(2u, second->generation());
    EXPECT_EQ(1u, second->items()[0].generation);
    EXPECT_EQ(2u, second->items()[1].generation);
    EXPECT_EQ(first->items()[0].value, second->items()[0].value);
    EXPECT_NE(first->items()[1].value, second->items()[1].value);

    EXPECT_EQ(2, *first->items()[1].value);
    EXPECT_EQ(20, *second->items()[1].value);
}

TEST(collection, swap)
{
    Collection<std::string> collection;
    auto transaction = collection.write();
    auto const items = transaction.items();

    transaction.pushBack("test1");
    transaction.pushBack("test2");
    transaction.pushBack("test3");

    auto const id1 = items.begin()->id;
    auto const id3 = (items.begin() + 2)->id;

    transaction.swap(items.begin(), items.begin() + 2);

    EXPECT_EQ("test3", transaction.values()[0]);
    EXPECT_EQ("test2", transaction.values()[1]);
    EXPECT_EQ("test1", transaction.values()[2]);
    EXPECT_EQ(id3, items.begin()->id);
    EXPECT_EQ(id1, (items.begin() + 2)->id);
}

TEST(collection, moveOneForward)
{
    Collection<std::string> collection;
    auto transaction = collection.write();
    auto const items = transaction.items();

    transaction.pushBack("test1");
    transaction.pushBack("test2");
    transaction.pushBack("test3");

    auto const id = items.begin()->id;
    transaction.move(items.begin(), items.begin() + 1);

    EXPECT_EQ("test2", transaction.values()[0]);
    EXPECT_EQ("test1", transaction.values()[1]);
    EXPECT_EQ("test3", transaction.values()[2]);
    EXPECT_EQ(id, (items.begin() + 1)->id);
}

TEST(collection, moveToLast)
{
    Collection<std::string> collection;
    auto transaction = collection.write();
    auto const items = transaction.items();

    transaction.pushBack("test1");
    transaction.pushBack("test2");
    transaction.pushBack("test3");

    transaction.move(items.begin(), items.end());

    EXPECT_EQ("test2", transaction.values()[0]);
    EXPECT_EQ("test3", transaction.values()[1]);
    EXPECT_EQ("test1", transaction.values()[2]);

    transaction.move(items.begin() + 1, items.end());

    EXPECT_EQ("test2", transaction.values()[0]);
    EXPECT_EQ("test1", transaction.values()[1]);
    EXPECT_EQ("test3", transaction.values()[2]);
}

TEST(collection, moveToBegin)
{
    Collection<std::string> collection;
    auto transaction = collection.write();
    auto const items = transaction.items();

    transaction.pushBack("test1");
    transaction.pushBack("test2");
    transaction.pushBack("test3");

    transaction.move(items.begin() + 1, items.begin());

    EXPECT_EQ("test2", transaction.values()[0]);
    EXPECT_EQ("test1", transaction.values()[1]);
    EXPECT_EQ("test3", transaction.values()[2]);

    transaction.move(items.begin() + 2, items.begin());

    EXPECT_EQ("test3", transaction.values()[0]);
    EXPECT_EQ("test2", transaction.values()[1]);
    EXPECT_EQ("test1", transaction.values()[2]);
}

TEST(collection, moveToSelfChangesNothing)
{
    Collection<std::string> collection;
    {
        auto transaction = collection.write();
        transaction.pushBack("test1");
        transaction.pushBack("test2");
        transaction.pushBack("test3");
    }

    {
        auto transaction = collection.write();
        auto const items = transaction.items();
        transaction.move(items.begin(), items.begin());
        transaction.move(items.begin() + 1, items.begin() + 1);
        transaction.move(items.begin() + 2, items.begin() + 2);
    }

    EXPECT_EQ(1u, collection.generation());
    EXPECT_EQ((std::vector<std::string>{ "test1", "test2", "test3" }),
            values(collection));
}

TEST(collection, sort)
{
    Collection<std::string> collection;
    {
        auto transaction = collection.write();
        transaction.pushBack("test3");
        transaction.pushBack("test2");
        transaction.pushBack("test1");
    }

    collection.write().sort();

    EXPECT_EQ((std::vector<std::string>{ "test1", "test2", "test3" }),
            values(collection));
    EXPECT_EQ(2u, collection.generation());

    // Sorting what is already sorted is not a change.
    collection.write().sort();
    EXPECT_EQ(2u, collection.generation());
}

// However many mutations one transaction makes, it publishes one snapshot with
// one new generation and notifies once, after the lock is released.
TEST(collection, oneTransactionIsOneGeneration)
{
    Collection<int> collection;

    std::vector<std::uint64_t> notified;
    auto connection = collection.onChange(
            [&notified, &collection](std::uint64_t generation)
            {
                notified.push_back(generation);

                // The write lock is released before notifying.
                collection.write();
            });

    EXPECT_EQ(0u, collection.generation());

    {
        auto transaction = collection.write();
        auto const items = transaction.items();
        for (int i = 0; i < 100; ++i)
            transaction.pushBack(i);
        transaction.update(items.begin(), -1);
        transaction.erase(items.begin() + 1);
        transaction.swap(items.begin(), items.begin() + 2);

        EXPECT_EQ(0u, collection.generation());
        EXPECT_TRUE(notified.empty());
    }

    EXPECT_EQ(1u, collection.generation());
    EXPECT_EQ(99u, collection.snapshot()->size());
    EXPECT_EQ((std::vector<std::uint64_t>{ 1 }), notified);
}

TEST(collection, aTransactionWithoutMutationPublishesNothing)
{
    Collection<int> collection;
    collection.write().pushBack(1);

    int notified = 0;
    auto connection = collection.onChange([&notified](std::uint64_t)
            {
                ++notified;
            });

    auto const before = collection.snapshot();
    {
        auto transaction = collection.write();
        transaction.findId(12345);
        transaction.eraseWithId(12345);
        EXPECT_EQ(1u, transaction.size());
    }

    EXPECT_EQ(before, collection.snapshot());
    EXPECT_EQ(0, notified);
}

TEST(collection, disconnectStopsNotifications)
{
    Collection<int> collection;

    int notified = 0;
    auto connection = collection.onChange([&notified](std::uint64_t)
            {
                ++notified;
            });

    collection.write().pushBack(1);
    EXPECT_EQ(1, notified);

    connection.disconnect();
    collection.write().pushBack(2);
    EXPECT_EQ(1, notified);
}

TEST(collection, copiesShareContents)
{
    Collection<int> a;
    Collection<int> b = a;

    a.write().pushBack(1);
    EXPECT_EQ((std::vector<int>{ 1 }), values(b));
    EXPECT_EQ(a.getId(), b.getId());
    EXPECT_NE(a.getId(), Collection<int>().getId());
}

// A reader on another thread is neither blocked by an open transaction nor
// shown its partial state.
TEST(collection, readersDoNotWaitForWriters)
{
    Collection<int> collection;
    collection.write().pushBack(1);

    std::promise<void> mutated;
    std::promise<void> read;
    auto readDone = read.get_future().share();

    std::thread writer([&collection, &mutated, readDone]()
        {
            auto transaction = collection.write();
            transaction.pushBack(2);
            transaction.update(transaction.items().begin(), 10);
            mutated.set_value();
            readDone.wait();
        });

    mutated.get_future().wait();

    auto const snapshot = collection.snapshot();
    EXPECT_EQ(1u, snapshot->generation());
    EXPECT_EQ((std::vector<int>{ 1 }), values(collection));

    read.set_value();
    writer.join();

    EXPECT_EQ(2u, collection.generation());
    EXPECT_EQ((std::vector<int>{ 10, 2 }), values(collection));

    // The old snapshot is unchanged.
    EXPECT_EQ(1u, snapshot->size());
    EXPECT_EQ(1, *snapshot->items()[0].value);
}

// Concurrent writers each publish whole transactions: every generation a
// reader sees is a consistent state, and generations only increase.
TEST(collection, concurrentWritersPublishWholeTransactions)
{
    Collection<int> collection;

    std::atomic<bool> done{ false };
    auto write = [&collection]()
        {
            for (int i = 0; i < 1000; ++i)
            {
                auto transaction = collection.write();
                transaction.pushBack(i);
                transaction.pushBack(-i);
            }
        };

    std::thread a(write);
    std::thread b(write);

    std::thread reader([&collection, &done]()
        {
            std::uint64_t last = 0;
            while (!done)
            {
                auto snapshot = collection.snapshot();
                EXPECT_LE(last, snapshot->generation());
                EXPECT_EQ(0u, snapshot->size() % 2);
                EXPECT_EQ(snapshot->generation() * 2, snapshot->size());
                last = snapshot->generation();
            }
        });

    a.join();
    b.join();
    done = true;
    reader.join();

    EXPECT_EQ(2000u, collection.generation());
    EXPECT_EQ(4000u, collection.snapshot()->size());
}

// An exception propagating out of a transaction discards all of its changes.
TEST(collection, anExceptionRollsBackTheTransaction)
{
    Collection<int> collection;
    collection.write().pushBack(1);

    int notified = 0;
    auto connection = collection.onChange([&notified](std::uint64_t)
            {
                ++notified;
            });

    auto const before = collection.snapshot();
    EXPECT_THROW(
            {
                auto transaction = collection.write();
                transaction.pushBack(2);
                transaction.update(transaction.items().begin(), 10);
                throw std::runtime_error("abort");
            },
            std::runtime_error);

    EXPECT_EQ(before, collection.snapshot());
    EXPECT_EQ(1u, collection.generation());
    EXPECT_EQ((std::vector<int>{ 1 }), values(collection));
    EXPECT_EQ(0, notified);

    // The lock was released.
    collection.write().pushBack(3);
    EXPECT_EQ((std::vector<int>{ 1, 3 }), values(collection));
    EXPECT_EQ(2u, collection.generation());
}

// A transaction opened while an exception is already in flight still publishes
// when it ends normally.
TEST(collection, aTransactionInsideUnwindingStillPublishes)
{
    Collection<int> collection;

    struct WriteOnDestruction
    {
        ~WriteOnDestruction()
        {
            collection.write().pushBack(1);
        }

        Collection<int>& collection;
    };

    try
    {
        WriteOnDestruction writer{ collection };
        throw std::runtime_error("unwind");
    }
    catch (std::runtime_error const&)
    {
    }

    EXPECT_EQ((std::vector<int>{ 1 }), values(collection));
}

TEST(collection, aThrowingCallbackDoesNotStopTheOthers)
{
    Collection<int> collection;

    int before = 0;
    int after = 0;
    auto first = collection.onChange([&before](std::uint64_t)
            {
                ++before;
            });
    auto throwing = collection.onChange([](std::uint64_t)
            {
                throw std::runtime_error("callback");
            });
    auto last = collection.onChange([&after](std::uint64_t)
            {
                ++after;
            });

    collection.write().pushBack(1);

    EXPECT_EQ(1, before);
    EXPECT_EQ(1, after);
    EXPECT_EQ(1u, collection.generation());
}

TEST(collection, noOpMutationsPublishNothing)
{
    Collection<std::string> collection;
    {
        auto transaction = collection.write();
        transaction.pushBack("a");
        transaction.pushBack("b");
        transaction.pushBack("c");
    }

    int notified = 0;
    auto connection = collection.onChange([&notified](std::uint64_t)
            {
                ++notified;
            });

    auto const before = collection.snapshot();
    {
        auto transaction = collection.write();
        auto const items = transaction.items();
        transaction.swap(items.begin(), items.begin());
        transaction.move(items.begin() + 2, items.end());
        transaction.move(items.begin() + 1, items.begin() + 1);
        transaction.sort();
    }

    EXPECT_EQ(before, collection.snapshot());
    EXPECT_EQ(0, notified);
}

namespace
{
    struct NoEq
    {
        int value;
    };

    // Has operator== declared for any T, but instantiating it does not
    // compile for 'std::vector<NoEq>'.
    using IllFormedEq = std::vector<NoEq>;

    template <typename T>
    void expectEveryUpdateIsAChange(T first, T second)
    {
        Collection<T> collection;
        collection.write().pushBack(first);

        int notified = 0;
        auto connection = collection.onChange([&notified](std::uint64_t)
                {
                    ++notified;
                });

        auto const before = collection.snapshot();
        {
            auto transaction = collection.write();
            transaction.update(transaction.items().begin(), second);
        }

        EXPECT_EQ(2u, collection.generation());
        EXPECT_EQ(1, notified);
        EXPECT_EQ(2u, collection.snapshot()->items()[0].generation);
        EXPECT_EQ(before->items()[0].id, collection.snapshot()->items()[0].id);
        EXPECT_NE(before->items()[0].value,
                collection.snapshot()->items()[0].value);
    }
} // namespace

TEST(collection, updateIsAlwaysAChange)
{
    expectEveryUpdateIsAChange(1, 1);
    expectEveryUpdateIsAChange(NoEq{ 1 }, NoEq{ 1 });
    expectEveryUpdateIsAChange(IllFormedEq{ NoEq{ 1 } },
            IllFormedEq{ NoEq{ 2 } });

    Collection<IllFormedEq> collection;
    {
        auto transaction = collection.write();
        transaction.pushBack({ NoEq{ 1 }, NoEq{ 2 } });
        transaction.update(transaction.items().begin(), { NoEq{ 3 } });
    }

    auto const values = collection.read().values();
    ASSERT_EQ(1u, values.size());
    ASSERT_EQ(1u, values[0].size());
    EXPECT_EQ(3, values[0][0].value);
}

namespace
{
    // Sorts a descending list with a comparator that throws part-way through,
    // catches that inside the transaction, then appends an item.
    void throwInsideSort(bool changedFirst)
    {
        Collection<int> collection;
        std::vector<int> expected;
        {
            auto transaction = collection.write();
            for (int i = 50; i > 0; --i)
            {
                transaction.pushBack(i);
                expected.push_back(i);
            }
        }

        {
            auto transaction = collection.write();
            if (changedFirst)
            {
                transaction.pushBack(0);
                expected.push_back(0);
            }

            int calls = 0;
            EXPECT_THROW(transaction.sort([&calls](int a, int b)
                        {
                            if (++calls > 10)
                                throw std::runtime_error("compare");

                            return a < b;
                        }),
                    std::runtime_error);

            transaction.pushBack(100);
            expected.push_back(100);
        }

        EXPECT_EQ(2u, collection.generation());
        for (auto const& item : collection.snapshot()->items())
            ASSERT_TRUE(item.value);

        EXPECT_EQ(expected, values(collection));
    }
} // namespace

// A comparator that throws, caught inside the transaction, leaves the items as
// they were before the sort, for the transaction's later mutations to build on.
TEST(collection, aThrowingSortChangesNothing)
{
    throwInsideSort(false);
    throwInsideSort(true);
}

TEST(collection, iteratorsAreRandomAccess)
{
    using Iterator = Collection<int>::View::Iterator;
    using ValueIterator = Collection<int>::View::ValueIterator;
    using WriteIterator = Collection<int>::Transaction::Iterator;
    using WriteValueIterator = Collection<int>::Transaction::ValueIterator;

    static_assert(std::is_same_v<std::random_access_iterator_tag,
            std::iterator_traits<Iterator>::iterator_category>);
    static_assert(std::is_same_v<std::random_access_iterator_tag,
            std::iterator_traits<ValueIterator>::iterator_category>);
    static_assert(std::is_same_v<std::random_access_iterator_tag,
            std::iterator_traits<WriteIterator>::iterator_category>);
    static_assert(std::is_same_v<std::random_access_iterator_tag,
            std::iterator_traits<WriteValueIterator>::iterator_category>);

    Collection<int> collection;
    {
        auto transaction = collection.write();
        for (int i = 0; i < 10; ++i)
            transaction.pushBack(i * 10);
    }

    auto view = collection.read();
    auto const items = view.items();

    Iterator i;
    i = items.begin();
    i += 3;
    EXPECT_EQ(30, *i->value);
    i -= 1;
    EXPECT_EQ(20, *(*i).value);
    EXPECT_EQ(50, *i[3].value);
    EXPECT_EQ(items.begin() + 4, 4 + items.begin());
    EXPECT_TRUE(items.begin() <= i);
    EXPECT_TRUE(i <= i);
    EXPECT_TRUE(items.end() >= i);
    EXPECT_FALSE(items.begin() >= i);

    auto j = items.begin();
    std::advance(j, 7);
    EXPECT_EQ(70, *j->value);
    EXPECT_EQ(7, std::distance(items.begin(), j));

    auto values = view.values();
    ValueIterator v;
    v = values.begin();
    v += 3;
    EXPECT_EQ(30, *v);
    v -= 1;
    EXPECT_EQ(20, *v);
    EXPECT_EQ(50, v[3]);
    EXPECT_EQ(values.begin() + 4, 4 + values.begin());
    EXPECT_TRUE(values.begin() <= v);
    EXPECT_TRUE(values.end() >= v);
    EXPECT_FALSE(values.begin() >= v);

    auto w = values.begin();
    std::advance(w, 7);
    EXPECT_EQ(70, *w);
    EXPECT_EQ(7, std::distance(values.begin(), w));

    auto k = std::lower_bound(values.begin(), values.end(), 45);
    EXPECT_EQ(50, *k);
    EXPECT_EQ(view.findId(k.base()->id), k.base());
    EXPECT_EQ(items.begin() + 5, k.base());

    auto byValue = [](CollectionItem<int> const& item, int value)
        {
            return *item.value < value;
        };
    EXPECT_EQ(k.base(),
            std::lower_bound(items.begin(), items.end(), 45, byValue));

    auto r = items.rbegin();
    r += 2;
    EXPECT_EQ(70, *r->value);
    EXPECT_EQ(60, *r[1].value);

    auto rv = values.rbegin();
    rv += 2;
    EXPECT_EQ(70, *rv);
    EXPECT_EQ(60, rv[1]);

    // Mutations accept positions found by algorithms over a transaction, as
    // item or value iterators.
    {
        auto transaction = collection.write();
        auto const writeItems = transaction.items();
        auto at = std::lower_bound(writeItems.begin(), writeItems.end(), 45,
                byValue);
        transaction.insert(at, 45);

        auto const writeValues = transaction.values();
        auto again = std::lower_bound(writeValues.begin(), writeValues.end(),
                55);
        transaction.insert(again, 55);
    }

    EXPECT_EQ(45, collection.read().values()[5]);
    EXPECT_EQ(55, collection.read().values()[7]);
}

// Reading through a non-const collection takes no lock, so it works inside a
// transaction on the same thread.
TEST(collection, readDoesNotTakeTheWriteLock)
{
    Collection<int> collection;
    auto transaction = collection.write();
    transaction.pushBack(1);

    EXPECT_EQ(0u, collection.read().size());
    EXPECT_TRUE(collection.read().empty());
    EXPECT_EQ(1u, transaction.size());
    EXPECT_FALSE(transaction.empty());
}

namespace
{
    template <typename T, typename = void>
    struct HasItems : std::false_type
    {
    };

    template <typename T>
    struct HasItems<T, std::void_t<decltype(std::declval<T>().items())>> :
        std::true_type
    {
    };

    template <typename T, typename = void>
    struct HasValues : std::false_type
    {
    };

    template <typename T>
    struct HasValues<T, std::void_t<decltype(std::declval<T>().values())>> :
        std::true_type
    {
    };

    template <typename T, typename = void>
    struct IsRange : std::false_type
    {
    };

    template <typename T>
    struct IsRange<T, std::void_t<decltype(std::begin(std::declval<T&>()))>> :
        std::true_type
    {
    };

    template <typename T, typename = void>
    struct HasIndex : std::false_type
    {
    };

    template <typename T>
    struct HasIndex<T, std::void_t<decltype(std::declval<T&>()[0])>> :
        std::true_type
    {
    };

    template <typename A, typename B, typename = void>
    struct IsEqualityComparable : std::false_type
    {
    };

    template <typename A, typename B>
    struct IsEqualityComparable<A, B, std::void_t<
        decltype(std::declval<A const&>() == std::declval<B const&>())>> :
        std::true_type
    {
    };
} // namespace

TEST(collection, onlyItemsAndValuesAreRanges)
{
    using Snapshot = Collection<std::string>::Snapshot;
    using View = Collection<std::string>::View;
    using Transaction = Collection<std::string>::Transaction;

    static_assert(!IsRange<Snapshot const>::value);
    static_assert(!IsRange<View>::value);
    static_assert(!IsRange<Transaction>::value);
    static_assert(!HasIndex<Snapshot const>::value);
    static_assert(!HasIndex<View>::value);
    static_assert(!HasIndex<Transaction>::value);

    static_assert(IsRange<Snapshot::Items>::value);
    static_assert(IsRange<Snapshot::Values>::value);
    static_assert(IsRange<Transaction::Items>::value);
    static_assert(IsRange<Transaction::Values>::value);

    // Ranges from a snapshot or a view own the snapshot; a transaction's
    // cannot own the lock.
    static_assert(HasItems<Snapshot const&>::value);
    static_assert(HasValues<Snapshot const&>::value);
    static_assert(HasItems<View>::value);
    static_assert(HasValues<View>::value);
    static_assert(HasItems<Transaction&>::value);
    static_assert(HasValues<Transaction&>::value);
    static_assert(!HasItems<Transaction>::value);
    static_assert(!HasValues<Transaction>::value);
}

TEST(collection, valueAndItemIteratorsDoNotMix)
{
    using View = Collection<int>::View;
    using Transaction = Collection<int>::Transaction;

    static_assert(IsEqualityComparable<View::Iterator,
            View::Iterator>::value);
    static_assert(IsEqualityComparable<View::ValueIterator,
            View::ValueIterator>::value);
    static_assert(!IsEqualityComparable<View::Iterator,
            View::ValueIterator>::value);
    static_assert(!IsEqualityComparable<View::ValueIterator,
            View::Iterator>::value);
    static_assert(!IsEqualityComparable<Transaction::Iterator,
            Transaction::ValueIterator>::value);
    static_assert(!IsEqualityComparable<Transaction::ValueIterator,
            Transaction::Iterator>::value);

    static_assert(!std::is_convertible_v<View::ValueIterator,
            View::Iterator>);
    static_assert(!std::is_convertible_v<View::Iterator,
            View::ValueIterator>);
    static_assert(!std::is_convertible_v<Transaction::ValueIterator,
            Transaction::Iterator>);
    static_assert(!std::is_convertible_v<Transaction::Iterator,
            Transaction::ValueIterator>);
}

TEST(collection, snapshotIteratesItemsAndValues)
{
    Collection<std::string> collection;
    {
        auto transaction = collection.write();
        transaction.pushBack("a");
        transaction.pushBack("b");
    }

    auto const snapshot = collection.snapshot();

    std::vector<std::uint64_t> itemIds;
    for (CollectionItem<std::string> const& item : snapshot->items())
        itemIds.push_back(item.id);

    EXPECT_EQ(ids(collection), itemIds);

    std::vector<std::string> itemValues;
    for (auto& value : snapshot->values())
        itemValues.push_back(value);

    EXPECT_EQ((std::vector<std::string>{ "a", "b" }), itemValues);

    auto const items = snapshot->items();
    EXPECT_EQ(2u, items.size());
    EXPECT_FALSE(items.empty());
    EXPECT_EQ(2, std::distance(std::begin(items), std::end(items)));
    EXPECT_EQ(&items[1], &*(items.begin() + 1));
    EXPECT_EQ(itemIds[1], items[1].id);
    EXPECT_EQ(snapshot->findId(itemIds[1]), items.begin() + 1);
    EXPECT_EQ(snapshot->findId(12345), items.end());

    auto const values = snapshot->values();
    EXPECT_EQ(2u, values.size());
    EXPECT_FALSE(values.empty());
    EXPECT_EQ("b", values[1]);
    EXPECT_EQ("a", *std::begin(values));
    EXPECT_EQ(2, std::distance(std::begin(values), std::end(values)));
}

TEST(collection, viewAndTransactionIterateItemsAndValues)
{
    Collection<std::string> collection;
    collection.write().pushBack("a");

    {
        auto transaction = collection.write();
        transaction.pushBack("b");

        std::vector<std::uint64_t> itemIds;
        std::vector<std::uint64_t> itemGenerations;
        for (auto const& item : transaction.items())
        {
            itemIds.push_back(item.id);
            itemGenerations.push_back(item.generation);
        }

        EXPECT_EQ(2u, itemIds.size());
        EXPECT_EQ((std::vector<std::uint64_t>{ 1, 2 }), itemGenerations);

        auto const items = transaction.items();
        EXPECT_EQ(transaction.findId(itemIds[1]), std::begin(items) + 1);
        EXPECT_EQ(transaction.findId(12345), items.end());
        EXPECT_EQ(itemIds[1], items[1].id);
        EXPECT_EQ("b", transaction.values()[1]);

        std::vector<std::string> itemValues;
        for (auto& value : transaction.values())
            itemValues.push_back(value);

        EXPECT_EQ((std::vector<std::string>{ "a", "b" }), itemValues);
    }

    std::vector<std::string> fromItems;
    for (auto const& item : collection.read().items())
        fromItems.push_back(*item.value);

    std::vector<std::string> fromValues;
    for (auto& value : collection.read().values())
        fromValues.push_back(value);

    EXPECT_EQ((std::vector<std::string>{ "a", "b" }), fromItems);
    EXPECT_EQ(fromItems, fromValues);

    auto view = collection.read();
    EXPECT_EQ(2u, view.items().size());
    EXPECT_EQ(2u, view.values().size());
    EXPECT_EQ("b", view.values()[1]);
    EXPECT_EQ("b", *view.items()[1].value);
}

// A range from a view or a snapshot keeps the snapshot alive, and its
// iterators valid, for as long as the range or a copy of it lives.
TEST(collection, rangesOwnTheirSnapshot)
{
    using View = Collection<std::string>::View;
    using Snapshot = Collection<std::string>::Snapshot;

    auto collection = std::make_unique<Collection<std::string>>();
    {
        auto transaction = collection->write();
        transaction.pushBack("a");
        transaction.pushBack("b");
    }

    std::optional<View::Values> values;
    std::optional<View::Items> items;
    {
        auto view = collection->read();
        auto const copy = view.values();
        values = copy;
        items = view.items();
    }

    auto const first = values->begin();
    auto const second = items->begin() + 1;

    auto fromSnapshot = [&collection]()
        {
            auto const snapshot = collection->snapshot();
            return snapshot->values();
        }();
    static_assert(std::is_same_v<Snapshot::Values, decltype(fromSnapshot)>);

    collection->write().pushBack("c");
    collection.reset();

    EXPECT_EQ((std::vector<std::string>{ "a", "b" }),
            std::vector<std::string>(values->begin(), values->end()));
    EXPECT_EQ("b", *(*items)[1].value);
    EXPECT_EQ((std::vector<std::string>{ "a", "b" }),
            std::vector<std::string>(fromSnapshot.begin(),
                fromSnapshot.end()));

    auto const lastValues = *values;
    values.reset();
    EXPECT_EQ("a", *first);
    EXPECT_EQ("a", lastValues[0]);

    auto const lastItems = *items;
    items.reset();
    EXPECT_EQ("b", *second->value);
    EXPECT_EQ(2u, lastItems.size());
}

// Mutations accept value iterators at the position they name.
TEST(collection, mutationsAcceptValueIterators)
{
    Collection<std::string> collection;
    {
        auto transaction = collection.write();
        transaction.pushBack("a");
        transaction.pushBack("b");
        transaction.pushBack("c");
    }

    {
        auto transaction = collection.write();
        auto values = transaction.values();
        transaction.erase(std::find(values.begin(), values.end(), "b"));

        values = transaction.values();
        transaction.update(values.begin(), "a2");

        values = transaction.values();
        transaction.move(values.begin(), values.end());
    }

    EXPECT_EQ((std::vector<std::string>{ "c", "a2" }), values(collection));
}

// 'iter_swap' on a transaction's iterators is the transaction's swap, whether
// called unqualified or through the 'using std::iter_swap' idiom.
TEST(collection, iterSwapSwapsThroughTheTransaction)
{
    Collection<std::string> collection;
    {
        auto transaction = collection.write();
        transaction.pushBack("a");
        transaction.pushBack("b");
        transaction.pushBack("c");
    }

    auto const before = ids(collection);

    int notified = 0;
    auto connection = collection.onChange([&notified](std::uint64_t)
            {
                ++notified;
            });

    {
        auto transaction = collection.write();
        auto const items = transaction.items();
        iter_swap(items.begin(), items.begin() + 2);

        using std::iter_swap;
        auto values = transaction.values();
        iter_swap(values.begin(), values.begin() + 1);

        EXPECT_EQ((std::vector<std::string>{ "b", "c", "a" }),
                std::vector<std::string>(transaction.values().begin(),
                    transaction.values().end()));
    }

    EXPECT_EQ(2u, collection.generation());
    EXPECT_EQ(1, notified);
    EXPECT_EQ((std::vector<std::string>{ "b", "c", "a" }), values(collection));
    EXPECT_EQ((std::vector<std::uint64_t>{ before[1], before[2], before[0] }),
            ids(collection));

    // Only reordered: every item keeps the generation it was inserted in.
    for (auto const& item : collection.snapshot()->items())
        EXPECT_EQ(1u, item.generation);

    // Swapping an item with itself is not a change.
    auto const unchanged = collection.snapshot();
    {
        auto transaction = collection.write();
        auto const items = transaction.items();
        using std::iter_swap;
        iter_swap(items.begin() + 1, items.begin() + 1);
        auto values = transaction.values();
        iter_swap(values.begin(), values.begin());
    }

    EXPECT_EQ(unchanged, collection.snapshot());
    EXPECT_EQ(1, notified);
}

namespace
{
    template <typename TIterator>
    void reverseBySwapping(TIterator first, TIterator last)
    {
        using std::iter_swap;
        while (first != last && first != --last)
        {
            iter_swap(first, last);
            ++first;
        }
    }
} // namespace

// Generic code that swaps through the 'using std::iter_swap' idiom reorders
// the collection within one transaction.
TEST(collection, genericIterSwapAlgorithm)
{
    Collection<std::string> collection;
    {
        auto transaction = collection.write();
        for (auto value : { "a", "b", "c", "d", "e" })
            transaction.pushBack(value);
    }

    {
        auto transaction = collection.write();
        auto const items = transaction.items();
        reverseBySwapping(items.begin(), items.end());
    }

    EXPECT_EQ((std::vector<std::string>{ "e", "d", "c", "b", "a" }),
            values(collection));
    EXPECT_EQ(2u, collection.generation());

    {
        auto transaction = collection.write();
        auto values = transaction.values();
        reverseBySwapping(values.begin(), values.end());
    }

    EXPECT_EQ((std::vector<std::string>{ "a", "b", "c", "d", "e" }),
            values(collection));
    EXPECT_EQ(3u, collection.generation());
}
