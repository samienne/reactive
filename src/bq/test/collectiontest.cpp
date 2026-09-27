#include <bq/signal/collection.h>

#include <gtest/gtest.h>

#include <atomic>
#include <future>
#include <string>
#include <thread>
#include <vector>

using namespace bq::signal;

namespace
{
    template <typename T>
    std::vector<T> values(Collection<T> const& collection)
    {
        auto range = collection.crangeLock();
        return std::vector<T>(range.begin(), range.end());
    }

    template <typename T>
    std::vector<size_t> ids(Collection<T> const& collection)
    {
        std::vector<size_t> result;
        auto range = collection.crangeLock();
        for (auto i = range.begin(); i != range.end(); ++i)
            result.push_back(i.getId());

        return result;
    }
} // namespace

TEST(collection, iterateForward)
{
    Collection<std::string> collection;
    auto range = collection.rangeLock();

    range.pushBack("test 1");
    range.pushBack("test 2");
    range.pushBack("test 3");

    EXPECT_EQ(3u, range.size());

    auto i = range.begin();
    EXPECT_EQ("test 1", *i);
    ++i;
    EXPECT_EQ("test 2", *i);
    ++i;
    EXPECT_EQ("test 3", *i);
    ++i;
    EXPECT_EQ(range.end(), i);
}

TEST(collection, iterateBackward)
{
    Collection<std::string> collection;
    auto range = collection.rangeLock();

    range.pushBack("test 1");
    range.pushBack("test 2");
    range.pushBack("test 3");

    auto i = range.rbegin();
    EXPECT_EQ("test 3", *i);
    ++i;
    EXPECT_EQ("test 2", *i);
    ++i;
    EXPECT_EQ("test 1", *i);
    ++i;
    EXPECT_EQ(range.rend(), i);
}

TEST(collection, iterateConst)
{
    Collection<std::string> col;
    {
        auto range = col.rangeLock();
        range.pushFront("test 1");
        range.pushFront("test 2");
        range.pushFront("test 3");
    }

    auto const& collection = col;
    auto range = collection.rangeLock();

    EXPECT_EQ(3u, range.size());
    EXPECT_EQ((std::vector<std::string>{ "test 3", "test 2", "test 1" }),
            std::vector<std::string>(range.begin(), range.end()));
    EXPECT_EQ((std::vector<std::string>{ "test 1", "test 2", "test 3" }),
            std::vector<std::string>(range.rbegin(), range.rend()));
}

TEST(collection, insert)
{
    Collection<int> collection;
    {
        auto range = collection.rangeLock();
        range.insert(range.begin(), 10);
        range.insert(range.end(), 20);
        range.insert(range.begin(), 30);
    }

    EXPECT_EQ((std::vector<int>{ 30, 10, 20 }), values(collection));
}

TEST(collection, idsAreStableAndNeverReused)
{
    Collection<std::string> collection;
    {
        auto range = collection.rangeLock();
        range.pushBack("a");
        range.pushBack("b");
    }

    auto const before = ids(collection);
    ASSERT_EQ(2u, before.size());
    EXPECT_NE(before[0], before[1]);

    {
        auto range = collection.rangeLock();
        range.update(range.begin(), "a2");
        range.eraseWithId(before[1]);
        range.pushBack("c");
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
        auto range = collection.rangeLock();
        range.pushBack(1);
        range.pushBack(2);
    }

    auto const first = collection.snapshot();
    EXPECT_EQ(1u, first->generation);

    {
        auto range = collection.rangeLock();
        range.update(range.begin() + 1, 20);
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
    auto range = collection.rangeLock();

    range.pushBack("test1");
    range.pushBack("test2");
    range.pushBack("test3");

    auto const id1 = range.begin().getId();
    auto const id3 = (range.begin() + 2).getId();

    range.swap(range.begin(), range.begin() + 2);

    EXPECT_EQ("test3", range[0]);
    EXPECT_EQ("test2", range[1]);
    EXPECT_EQ("test1", range[2]);
    EXPECT_EQ(id3, range.begin().getId());
    EXPECT_EQ(id1, (range.begin() + 2).getId());
}

TEST(collection, moveOneForward)
{
    Collection<std::string> collection;
    auto range = collection.rangeLock();

    range.pushBack("test1");
    range.pushBack("test2");
    range.pushBack("test3");

    auto const id = range.begin().getId();
    range.move(range.begin(), range.begin() + 1);

    EXPECT_EQ("test2", range[0]);
    EXPECT_EQ("test1", range[1]);
    EXPECT_EQ("test3", range[2]);
    EXPECT_EQ(id, (range.begin() + 1).getId());
}

TEST(collection, moveToLast)
{
    Collection<std::string> collection;
    auto range = collection.rangeLock();

    range.pushBack("test1");
    range.pushBack("test2");
    range.pushBack("test3");

    range.move(range.begin(), range.end());

    EXPECT_EQ("test2", range[0]);
    EXPECT_EQ("test3", range[1]);
    EXPECT_EQ("test1", range[2]);

    range.move(range.begin() + 1, range.end());

    EXPECT_EQ("test2", range[0]);
    EXPECT_EQ("test1", range[1]);
    EXPECT_EQ("test3", range[2]);
}

TEST(collection, moveToBegin)
{
    Collection<std::string> collection;
    auto range = collection.rangeLock();

    range.pushBack("test1");
    range.pushBack("test2");
    range.pushBack("test3");

    range.move(range.begin() + 1, range.begin());

    EXPECT_EQ("test2", range[0]);
    EXPECT_EQ("test1", range[1]);
    EXPECT_EQ("test3", range[2]);

    range.move(range.begin() + 2, range.begin());

    EXPECT_EQ("test3", range[0]);
    EXPECT_EQ("test2", range[1]);
    EXPECT_EQ("test1", range[2]);
}

TEST(collection, moveToSelfChangesNothing)
{
    Collection<std::string> collection;
    {
        auto range = collection.rangeLock();
        range.pushBack("test1");
        range.pushBack("test2");
        range.pushBack("test3");
    }

    {
        auto range = collection.rangeLock();
        range.move(range.begin(), range.begin());
        range.move(range.begin() + 1, range.begin() + 1);
        range.move(range.begin() + 2, range.begin() + 2);
    }

    EXPECT_EQ(1u, collection.generation());
    EXPECT_EQ((std::vector<std::string>{ "test1", "test2", "test3" }),
            values(collection));
}

TEST(collection, sort)
{
    Collection<std::string> collection;
    {
        auto range = collection.rangeLock();
        range.pushBack("test3");
        range.pushBack("test2");
        range.pushBack("test1");
    }

    collection.rangeLock().sort();

    EXPECT_EQ((std::vector<std::string>{ "test1", "test2", "test3" }),
            values(collection));
    EXPECT_EQ(2u, collection.generation());

    // Sorting what is already sorted is not a change.
    collection.rangeLock().sort();
    EXPECT_EQ(2u, collection.generation());
}

// However many mutations one transaction makes, it publishes one snapshot with
// one new generation and notifies once, after the lock is released.
TEST(collection, oneTransactionIsOneGeneration)
{
    Collection<int> collection;

    std::vector<uint64_t> notified;
    auto connection = collection.onChange(
            [&notified, &collection](uint64_t generation)
            {
                notified.push_back(generation);

                // The write lock is released before notifying.
                collection.rangeLock();
            });

    EXPECT_EQ(0u, collection.generation());

    {
        auto range = collection.rangeLock();
        for (int i = 0; i < 100; ++i)
            range.pushBack(i);
        range.update(range.begin(), -1);
        range.erase(range.begin() + 1);
        range.swap(range.begin(), range.begin() + 2);

        EXPECT_EQ(0u, collection.generation());
        EXPECT_TRUE(notified.empty());
    }

    EXPECT_EQ(1u, collection.generation());
    EXPECT_EQ(99u, collection.snapshot()->items.size());
    EXPECT_EQ((std::vector<uint64_t>{ 1 }), notified);
}

TEST(collection, aTransactionWithoutMutationPublishesNothing)
{
    Collection<int> collection;
    collection.rangeLock().pushBack(1);

    int notified = 0;
    auto connection = collection.onChange([&notified](uint64_t)
            {
                ++notified;
            });

    auto const before = collection.snapshot();
    {
        auto range = collection.rangeLock();
        range.findId(12345);
        range.eraseWithId(12345);
        EXPECT_EQ(1u, range.size());
    }

    EXPECT_EQ(before, collection.snapshot());
    EXPECT_EQ(0, notified);
}

TEST(collection, disconnectStopsNotifications)
{
    Collection<int> collection;

    int notified = 0;
    auto connection = collection.onChange([&notified](uint64_t)
            {
                ++notified;
            });

    collection.rangeLock().pushBack(1);
    EXPECT_EQ(1, notified);

    connection.disconnect();
    collection.rangeLock().pushBack(2);
    EXPECT_EQ(1, notified);
}

TEST(collection, copiesShareContents)
{
    Collection<int> a;
    Collection<int> b = a;

    a.rangeLock().pushBack(1);
    EXPECT_EQ((std::vector<int>{ 1 }), values(b));
    EXPECT_EQ(a.getId(), b.getId());
    EXPECT_NE(a.getId(), Collection<int>().getId());
}

// A reader on another thread is neither blocked by an open transaction nor
// shown its partial state.
TEST(collection, readersDoNotWaitForWriters)
{
    Collection<int> collection;
    collection.rangeLock().pushBack(1);

    std::promise<void> mutated;
    std::promise<void> read;
    auto readDone = read.get_future().share();

    std::thread writer([&collection, &mutated, readDone]()
        {
            auto range = collection.rangeLock();
            range.pushBack(2);
            range.update(range.begin(), 10);
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
                auto range = collection.rangeLock();
                range.pushBack(i);
                range.pushBack(-i);
            }
        };

    std::thread a(write);
    std::thread b(write);

    std::thread reader([&collection, &done]()
        {
            uint64_t last = 0;
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
