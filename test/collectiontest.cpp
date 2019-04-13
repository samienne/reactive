#include <reactive/collection.h>

#include <gtest/gtest.h>

#include <iostream>

using namespace reactive;

TEST(reactiveCollection, iterateForward)
{
    Collection<std::string> collection;
    auto range = collection.rangeLock();

    range.pushBack("test 1");
    range.pushBack("test 2");
    range.pushBack("test 3");

    EXPECT_EQ(3, range.size());

    auto i = range.begin();

    EXPECT_EQ("test 1", *i);

    ++i;
    EXPECT_EQ("test 2", *i);

    ++i;
    EXPECT_EQ("test 3", *i);

    ++i;
    EXPECT_EQ(range.end(), i);
}


TEST(reactiveCollection, iterateBackward)
{
    Collection<std::string> collection;
    auto range = collection.rangeLock();

    range.pushBack("test 1");
    range.pushBack("test 2");
    range.pushBack("test 3");

    EXPECT_EQ(3, range.size());

    auto i = range.rbegin();

    EXPECT_EQ("test 3", *i);

    ++i;
    EXPECT_EQ("test 2", *i);

    ++i;
    EXPECT_EQ("test 1", *i);

    ++i;
    EXPECT_EQ(range.rend(), i);
}

TEST(reactiveCollection, iterateForwardConst)
{
    Collection<std::string> col;

    {
        auto range = col.rangeLock();

        range.pushBack("test 1");
        range.pushBack("test 2");
        range.pushBack("test 3");
    }

    auto const& collection = col;

    auto range = collection.rangeLock();

    EXPECT_EQ(3, range.size());

    auto i = range.begin();

    EXPECT_EQ("test 1", *i);

    ++i;
    EXPECT_EQ("test 2", *i);

    ++i;
    EXPECT_EQ("test 3", *i);

    ++i;
    EXPECT_EQ(range.end(), i);
}

TEST(reactiveCollection, iterateBackwardConst)
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

    EXPECT_EQ(3, range.size());

    auto i = range.rbegin();

    EXPECT_EQ("test 1", *i);

    ++i;
    EXPECT_EQ("test 2", *i);

    ++i;
    EXPECT_EQ("test 3", *i);

    ++i;
    EXPECT_EQ(range.rend(), i);
}

TEST(reactiveCollection, insert)
{
    Collection<int> collection;
    auto range = collection.rangeLock();

    range.insert(range.begin(), 10);
    range.insert(range.end(), 20);
    range.insert(range.begin(), 30);

    auto i = range.begin();
    EXPECT_EQ(30, *i);

    ++i;
    EXPECT_EQ(10, *i);

    ++i;
    EXPECT_EQ(20, *i);
}

TEST(reactiveCollection, callbacks)
{
    Collection<std::string> collection;

    int inserts = 0;
    int updates = 0;
    int erases = 0;

    size_t lastInsertId = 0;
    size_t lastUpdateId = 0;
    size_t lastEraseId = 0;

    std::string lastInsertValue;
    std::string lastUpdateValue;

    Connection connections;

    connections += collection.onInsert(
            [&inserts, &lastInsertId, &lastInsertValue]
            (auto id, int, auto const& value)
            {
                lastInsertId = id;
                lastInsertValue = value;
                ++inserts;
            });

    connections += collection.onUpdate(
            [&updates, &lastUpdateId, &lastUpdateValue]
            (auto id, int, auto const& value)
            {
                lastUpdateId = id;
                lastUpdateValue = value;
                ++updates;
            });

    connections += collection.onErase(
            [&erases, &lastEraseId](auto id)
            {
                lastEraseId = id;
                ++erases;
            });

    auto range = collection.rangeLock();

    range.pushBack("item 1");

    EXPECT_EQ(1, inserts);
    EXPECT_EQ(0, updates);
    EXPECT_EQ(0, erases);

    EXPECT_EQ("item 1", lastInsertValue);
    auto item1Id = lastInsertId;

    range.pushBack("item 2");

    EXPECT_EQ(2, inserts);
    EXPECT_EQ(0, updates);
    EXPECT_EQ(0, erases);

    EXPECT_EQ("item 2", lastInsertValue);

    range.update(range.begin(), "item 3");

    EXPECT_EQ(1, updates);

    EXPECT_EQ("item 3", lastUpdateValue);
    EXPECT_EQ(item1Id, lastUpdateId);

    EXPECT_EQ(2, range.size());

    auto i = range.begin();
    EXPECT_EQ("item 3", *i);

    ++i;
    EXPECT_EQ("item 2", *i);

    range.erase(range.begin());

    EXPECT_EQ(1, erases);
    EXPECT_EQ(1, range.size());
    EXPECT_EQ("item 2", *range.begin());
    EXPECT_EQ(item1Id, lastEraseId);
}

TEST(reactiveCollection, swap)
{
    Collection<std::string> collection;

    size_t lastId1 = 0;
    size_t lastIndex1 = 12345;
    size_t lastId2 = 0;
    size_t lastIndex2 = 12345;

    auto connection = collection.onSwap(
            [&](size_t id1, int index1, size_t id2, int index2)
            {
                lastId1 = id1;
                lastIndex1 = index1;
                lastId2 = id2;
                lastIndex2 = index2;
            });

    auto range = collection.rangeLock();

    range.pushBack("test1");
    range.pushBack("test2");
    range.pushBack("test3");

    auto i = range.begin();
    auto j = range.begin() + 2;

    range.swap(i, j);

    // Check that callback told that 0th and 2nd elements were swapped.
    EXPECT_EQ((range.begin()+2).getId(), lastId1);
    EXPECT_EQ((range.begin()).getId(), lastId2);
    EXPECT_EQ(0, lastIndex1);
    EXPECT_EQ(2, lastIndex2);

    EXPECT_EQ("test3", range[0]);
    EXPECT_EQ("test2", range[1]);
    EXPECT_EQ("test1", range[2]);
}

