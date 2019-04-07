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

