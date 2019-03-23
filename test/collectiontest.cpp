#include <reactive/collection.h>

#include <gtest/gtest.h>

#include <iostream>

using namespace reactive;

TEST(reactiveCollection, iterateForward)
{
    Collection<std::string> collection;

    collection.pushBack("test 1");
    collection.pushBack("test 2");
    collection.pushBack("test 3");

    EXPECT_EQ(3, collection.size());

    auto i = collection.begin();

    EXPECT_EQ("test 1", *i);

    ++i;
    EXPECT_EQ("test 2", *i);

    ++i;
    EXPECT_EQ("test 3", *i);

    ++i;
    EXPECT_EQ(collection.end(), i);
}


TEST(reactiveCollection, iterateBackward)
{
    Collection<std::string> collection;

    collection.pushBack("test 1");
    collection.pushBack("test 2");
    collection.pushBack("test 3");

    EXPECT_EQ(3, collection.size());

    auto i = collection.rbegin();

    EXPECT_EQ("test 3", *i);

    ++i;
    EXPECT_EQ("test 2", *i);

    ++i;
    EXPECT_EQ("test 1", *i);

    ++i;
    EXPECT_EQ(collection.rend(), i);
}

TEST(reactiveCollection, iterateForwardConst)
{
    Collection<std::string> col;

    col.pushBack("test 1");
    col.pushBack("test 2");
    col.pushBack("test 3");

    auto const& collection = col;

    EXPECT_EQ(3, collection.size());

    auto i = collection.begin();

    EXPECT_EQ("test 1", *i);

    ++i;
    EXPECT_EQ("test 2", *i);

    ++i;
    EXPECT_EQ("test 3", *i);

    ++i;
    EXPECT_EQ(collection.end(), i);
}

TEST(reactiveCollection, iterateBackwardConst)
{
    Collection<std::string> col;

    col.pushFront("test 1");
    col.pushFront("test 2");
    col.pushFront("test 3");

    auto const& collection = col;

    EXPECT_EQ(3, collection.size());

    auto i = collection.rbegin();

    EXPECT_EQ("test 1", *i);

    ++i;
    EXPECT_EQ("test 2", *i);

    ++i;
    EXPECT_EQ("test 3", *i);

    ++i;
    EXPECT_EQ(collection.rend(), i);
}

TEST(reactiveCollection, insert)
{
    Collection<int> collection;

    collection.insert(collection.begin(), 10);
    collection.insert(collection.end(), 20);
    collection.insert(collection.begin(), 30);


    auto i = collection.begin();
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
            (auto id, auto const& value)
            {
                lastInsertId = id;
                lastInsertValue = value;
                ++inserts;
            });

    connections += collection.onUpdate(
            [&updates, &lastUpdateId, &lastUpdateValue]
            (auto id, auto const& value)
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

    collection.pushBack("item 1");

    EXPECT_EQ(1, inserts);
    EXPECT_EQ(0, updates);
    EXPECT_EQ(0, erases);

    EXPECT_EQ("item 1", lastInsertValue);
    auto item1Id = lastInsertId;

    collection.pushBack("item 2");

    EXPECT_EQ(2, inserts);
    EXPECT_EQ(0, updates);
    EXPECT_EQ(0, erases);

    EXPECT_EQ("item 2", lastInsertValue);

    collection.update(collection.begin(), "item 3");

    EXPECT_EQ(1, updates);

    EXPECT_EQ("item 3", lastUpdateValue);
    EXPECT_EQ(item1Id, lastUpdateId);

    EXPECT_EQ(2, collection.size());

    auto i = collection.begin();
    EXPECT_EQ("item 3", *i);

    ++i;
    EXPECT_EQ("item 2", *i);

    collection.erase(collection.begin());

    EXPECT_EQ(1, erases);
    EXPECT_EQ(1, collection.size());
    EXPECT_EQ("item 2", *collection.begin());
    EXPECT_EQ(item1Id, lastEraseId);
}

