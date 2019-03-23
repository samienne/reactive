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

