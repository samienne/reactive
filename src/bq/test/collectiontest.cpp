#include <bq/signal/collection.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <future>
#include <iterator>
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
        auto view = collection.read();
        return std::vector<T>(view.begin(), view.end());
    }

    template <typename T>
    std::vector<std::uint64_t> ids(Collection<T> const& collection)
    {
        std::vector<std::uint64_t> result;
        auto view = collection.read();
        for (auto i = view.begin(); i != view.end(); ++i)
            result.push_back(i.getId());

        return result;
    }
} // namespace

TEST(collection, iterateForward)
{
    Collection<std::string> collection;
    auto transaction = collection.write();

    transaction.pushBack("test 1");
    transaction.pushBack("test 2");
    transaction.pushBack("test 3");

    EXPECT_EQ(3u, transaction.size());

    auto i = transaction.begin();
    EXPECT_EQ("test 1", *i);
    ++i;
    EXPECT_EQ("test 2", *i);
    ++i;
    EXPECT_EQ("test 3", *i);
    ++i;
    EXPECT_EQ(transaction.end(), i);
}

TEST(collection, iterateBackward)
{
    Collection<std::string> collection;
    auto transaction = collection.write();

    transaction.pushBack("test 1");
    transaction.pushBack("test 2");
    transaction.pushBack("test 3");

    auto i = transaction.rbegin();
    EXPECT_EQ("test 3", *i);
    ++i;
    EXPECT_EQ("test 2", *i);
    ++i;
    EXPECT_EQ("test 1", *i);
    ++i;
    EXPECT_EQ(transaction.rend(), i);
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
    EXPECT_EQ((std::vector<std::string>{ "test 3", "test 2", "test 1" }),
            std::vector<std::string>(view.begin(), view.end()));
    EXPECT_EQ((std::vector<std::string>{ "test 1", "test 2", "test 3" }),
            std::vector<std::string>(view.rbegin(), view.rend()));
}

TEST(collection, insert)
{
    Collection<int> collection;
    {
        auto transaction = collection.write();
        transaction.insert(transaction.begin(), 10);
        transaction.insert(transaction.end(), 20);
        transaction.insert(transaction.begin(), 30);
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
        transaction.update(transaction.begin(), "a2");
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
    EXPECT_EQ(1u, first->generation);

    {
        auto transaction = collection.write();
        transaction.update(transaction.begin() + 1, 20);
    }

    auto const second = collection.snapshot();
    EXPECT_EQ(2u, second->generation);
    EXPECT_EQ(1u, second->items[0].generation);
    EXPECT_EQ(2u, second->items[1].generation);
    EXPECT_EQ(first->items[0].value, second->items[0].value);
    EXPECT_NE(first->items[1].value, second->items[1].value);

    EXPECT_EQ(2, *first->items[1].value);
    EXPECT_EQ(20, *second->items[1].value);
}

TEST(collection, swap)
{
    Collection<std::string> collection;
    auto transaction = collection.write();

    transaction.pushBack("test1");
    transaction.pushBack("test2");
    transaction.pushBack("test3");

    auto const id1 = transaction.begin().getId();
    auto const id3 = (transaction.begin() + 2).getId();

    transaction.swap(transaction.begin(), transaction.begin() + 2);

    EXPECT_EQ("test3", transaction[0]);
    EXPECT_EQ("test2", transaction[1]);
    EXPECT_EQ("test1", transaction[2]);
    EXPECT_EQ(id3, transaction.begin().getId());
    EXPECT_EQ(id1, (transaction.begin() + 2).getId());
}

TEST(collection, moveOneForward)
{
    Collection<std::string> collection;
    auto transaction = collection.write();

    transaction.pushBack("test1");
    transaction.pushBack("test2");
    transaction.pushBack("test3");

    auto const id = transaction.begin().getId();
    transaction.move(transaction.begin(), transaction.begin() + 1);

    EXPECT_EQ("test2", transaction[0]);
    EXPECT_EQ("test1", transaction[1]);
    EXPECT_EQ("test3", transaction[2]);
    EXPECT_EQ(id, (transaction.begin() + 1).getId());
}

TEST(collection, moveToLast)
{
    Collection<std::string> collection;
    auto transaction = collection.write();

    transaction.pushBack("test1");
    transaction.pushBack("test2");
    transaction.pushBack("test3");

    transaction.move(transaction.begin(), transaction.end());

    EXPECT_EQ("test2", transaction[0]);
    EXPECT_EQ("test3", transaction[1]);
    EXPECT_EQ("test1", transaction[2]);

    transaction.move(transaction.begin() + 1, transaction.end());

    EXPECT_EQ("test2", transaction[0]);
    EXPECT_EQ("test1", transaction[1]);
    EXPECT_EQ("test3", transaction[2]);
}

TEST(collection, moveToBegin)
{
    Collection<std::string> collection;
    auto transaction = collection.write();

    transaction.pushBack("test1");
    transaction.pushBack("test2");
    transaction.pushBack("test3");

    transaction.move(transaction.begin() + 1, transaction.begin());

    EXPECT_EQ("test2", transaction[0]);
    EXPECT_EQ("test1", transaction[1]);
    EXPECT_EQ("test3", transaction[2]);

    transaction.move(transaction.begin() + 2, transaction.begin());

    EXPECT_EQ("test3", transaction[0]);
    EXPECT_EQ("test2", transaction[1]);
    EXPECT_EQ("test1", transaction[2]);
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
        transaction.move(transaction.begin(), transaction.begin());
        transaction.move(transaction.begin() + 1, transaction.begin() + 1);
        transaction.move(transaction.begin() + 2, transaction.begin() + 2);
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
        for (int i = 0; i < 100; ++i)
            transaction.pushBack(i);
        transaction.update(transaction.begin(), -1);
        transaction.erase(transaction.begin() + 1);
        transaction.swap(transaction.begin(), transaction.begin() + 2);

        EXPECT_EQ(0u, collection.generation());
        EXPECT_TRUE(notified.empty());
    }

    EXPECT_EQ(1u, collection.generation());
    EXPECT_EQ(99u, collection.snapshot()->items.size());
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
            transaction.update(transaction.begin(), 10);
            mutated.set_value();
            readDone.wait();
        });

    mutated.get_future().wait();

    auto const snapshot = collection.snapshot();
    EXPECT_EQ(1u, snapshot->generation);
    EXPECT_EQ((std::vector<int>{ 1 }), values(collection));

    read.set_value();
    writer.join();

    EXPECT_EQ(2u, collection.generation());
    EXPECT_EQ((std::vector<int>{ 10, 2 }), values(collection));

    // The old snapshot is unchanged.
    EXPECT_EQ(1u, snapshot->items.size());
    EXPECT_EQ(1, *snapshot->items[0].value);
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
                EXPECT_LE(last, snapshot->generation);
                EXPECT_EQ(0u, snapshot->items.size() % 2);
                EXPECT_EQ(snapshot->generation * 2, snapshot->items.size());
                last = snapshot->generation;
            }
        });

    a.join();
    b.join();
    done = true;
    reader.join();

    EXPECT_EQ(2000u, collection.generation());
    EXPECT_EQ(4000u, collection.snapshot()->items.size());
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
                transaction.update(transaction.begin(), 10);
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
        transaction.swap(transaction.begin(), transaction.begin());
        transaction.move(transaction.begin() + 2, transaction.end());
        transaction.move(transaction.begin() + 1, transaction.begin() + 1);
        transaction.update(transaction.begin() + 1, "b");
        transaction.sort();
    }

    EXPECT_EQ(before, collection.snapshot());
    EXPECT_EQ(0, notified);
}

TEST(collection, updatingAValueWithoutEqualityIsAChange)
{
    struct Opaque
    {
        int value;
    };

    Collection<Opaque> collection;
    collection.write().pushBack(Opaque{ 1 });

    {
        auto transaction = collection.write();
        transaction.update(transaction.begin(), Opaque{ 1 });
    }

    EXPECT_EQ(2u, collection.generation());
}

TEST(collection, iteratorsAreRandomAccess)
{
    using Iterator = Collection<int>::Iterator;
    static_assert(std::is_same_v<std::random_access_iterator_tag,
            std::iterator_traits<Iterator>::iterator_category>);

    Collection<int> collection;
    {
        auto transaction = collection.write();
        for (int i = 0; i < 10; ++i)
            transaction.pushBack(i * 10);
    }

    auto view = collection.read();

    Iterator i;
    i = view.begin();
    i += 3;
    EXPECT_EQ(30, *i);
    i -= 1;
    EXPECT_EQ(20, *i);
    EXPECT_EQ(50, i[3]);
    EXPECT_EQ(view.begin() + 4, 4 + view.begin());
    EXPECT_TRUE(view.begin() <= i);
    EXPECT_TRUE(i <= i);
    EXPECT_TRUE(view.end() >= i);
    EXPECT_FALSE(view.begin() >= i);

    auto j = view.begin();
    std::advance(j, 7);
    EXPECT_EQ(70, *j);
    EXPECT_EQ(7, std::distance(view.begin(), j));

    auto k = std::lower_bound(view.begin(), view.end(), 45);
    EXPECT_EQ(50, *k);
    EXPECT_EQ(view.findId(k.getId()), k);

    auto r = view.rbegin();
    r += 2;
    EXPECT_EQ(70, *r);
    EXPECT_EQ(60, r[1]);

    // Mutations accept positions found by algorithms over a transaction.
    {
        auto transaction = collection.write();
        auto at = std::lower_bound(transaction.begin(), transaction.end(), 45);
        transaction.insert(at, 45);
    }

    EXPECT_EQ(45, collection.read()[5]);
}

// Reading through a non-const collection takes no lock, so it works inside a
// transaction on the same thread.
TEST(collection, readDoesNotTakeTheWriteLock)
{
    Collection<int> collection;
    auto transaction = collection.write();
    transaction.pushBack(1);

    EXPECT_EQ(0u, collection.read().size());
    EXPECT_EQ(1u, transaction.size());
}
