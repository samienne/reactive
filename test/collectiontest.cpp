#include <btl/mappedcollection.h>
#include <btl/collection.h>

#include <gtest/gtest.h>

#include <iostream>

using namespace btl;

TEST(collection, construct)
{
    collection<int> c1;
    collection<int> c2 = c1;;
    collection<int> c3 = std::move(c1);
}

TEST(collection, push)
{
    bool called = false;

    collection<int> c1;
    auto c = c1.on_changed([&c1, &called](collection_event<int> const& e)
        {
            called = true;
            std::array<int, 3> a{{10, 20, 30}};

            size_t i = 0;
            for (auto&& v : e.added)
            {
                EXPECT_TRUE(a[i++] == *v.second);
            }
        });

    {
        auto w = c1.writer();
        w.push_back(10);
        w.push_back(20);
        w.push_back(30);
    }

    EXPECT_TRUE(called);

    std::array<int, 3> a{{10, 20, 30}};

    auto r = c1.reader();
    size_t i = 0;
    for (auto&& v : r)
    {
        EXPECT_TRUE(a[i++] == v);
    }
}

TEST(collection, erase)
{
    collection<int> c;

    {
        auto w = c.writer();
        w.push_back(10);
        w.push_back(20);
        w.push_back(30);
    }

    bool called = false;
    auto connection = c.on_changed([&called](collection_event<int> const& e)
        {
            EXPECT_EQ(1, e.removed.size());
            EXPECT_EQ(0, e.removed[0]);
            called = true;
        });

    {
        auto w = c.writer();
        w.erase(w.begin());
    }

    EXPECT_TRUE(called);

    std::array<int, 2> a{{20, 30}};

    auto r = c.reader();
    size_t i = 0;
    for (auto&& v : r)
    {
        EXPECT_TRUE(a[i++] == v);
    }
}

TEST(collection, pushAndErase)
{
    collection<int> c;

    {
        auto w = c.writer();
        w.push_back(10);
        w.push_back(20);
        w.push_back(30);
    }

    bool called = false;
    auto connection = c.on_changed([&called](collection_event<int> const& e)
        {
            EXPECT_EQ(1, e.removed.size());
            EXPECT_EQ(1, e.added.size());
            EXPECT_EQ(0, e.removed[0]);
            EXPECT_EQ(2, e.added[0].first);
            called = true;
        });

    {
        auto w = c.writer();
        w.push_back(40);
        w.erase(w.begin());
    }

    EXPECT_TRUE(called);

    std::array<int, 3> a{{20, 30, 40}};

    auto r = c.reader();
    size_t i = 0;
    for (auto&& v : r)
    {
        EXPECT_TRUE(a[i++] == v);
    }
}

TEST(collection, eraseAndPush)
{
    collection<int> c;

    {
        auto w = c.writer();
        w.push_back(10);
        w.push_back(20);
        w.push_back(30);
    }

    bool called = false;
    auto connection = c.on_changed([&called](collection_event<int> const& e)
        {
            EXPECT_EQ(1, e.removed.size());
            EXPECT_EQ(1, e.added.size());
            EXPECT_EQ(0, e.removed[0]);
            EXPECT_EQ(2, e.added[0].first);
            called = true;
        });

    {
        auto w = c.writer();
        w.erase(w.begin());
        w.push_back(40);
    }

    EXPECT_TRUE(called);

    std::array<int, 3> a{{20, 30, 40}};

    auto r = c.reader();
    size_t i = 0;
    for (auto&& v : r)
    {
        EXPECT_TRUE(a[i++] == v);
    }
}

TEST(collection, update)
{
    collection<int> c;

    {
        auto w = c.writer();
        w.push_back(10);
        w.push_back(20);
        w.push_back(30);
    }

    bool called = false;
    auto connection = c.on_changed([&called](collection_event<int> const& e)
        {
            EXPECT_EQ(3, e.updated.size());
            EXPECT_EQ(0, e.updated[0].first);
            called = true;
        });

    {
        auto w = c.writer();
        w[0] = 30;
        w[1] = 40;
        w[2] = 50;
    }

    EXPECT_TRUE(called);

    std::array<int, 3> a{{30, 40, 50}};

    auto r = c.reader();
    size_t i = 0;
    for (auto&& v : r)
    {
        EXPECT_TRUE(a[i++] == v);
    }
}

/*TEST(collection, mapped)
{
    collection<int> c;

    {
        auto w = c.writer();
        for (int i = 0; i < 100; ++i)
            w.push_back(1000 - i * 10);
    }

    auto c2 = map_collection([](int i, size_t) { return i * 2; }, c);

    auto connection = observe(c2, [](collection_event<int> const&)
            {
            });

    auto r = c2.reader();

    EXPECT_EQ(100, r.size());

    size_t i = 0;
    for (auto&& v : r)
    {
        auto expected = (1000 - i * 10) * 2;
        EXPECT_EQ(expected, v);
        ++i;
    }
}*/

/*TEST(collection, mappedErase)
{
    collection<int> c;

    {
        auto w = c.writer();
        for (int i = 0; i < 5; ++i)
            w.push_back(50 - i * 10);
    }

    auto c2 = map_collection([](int i, size_t) { return i * 2; }, c);

    {
        auto w = c.writer();
        auto i = w.begin();
        w.erase(i++);
        w.erase(i++);
        w.erase(i++);
    }

    auto r = c2.reader();

    EXPECT_EQ(2, r.size());
    EXPECT_EQ(2, c.reader().size());

    size_t i = 3;
    for (auto&& v : r)
    {
        auto expected = (50 - i * 10) * 2;
        EXPECT_EQ(expected, v);
        ++i;
    }
}*/

