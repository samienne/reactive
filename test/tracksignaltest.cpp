#include <reactive/signal/map.h>
#include <reactive/signal/track.h>

#include <btl/mappedcollection.h>
#include <btl/collection.h>

#include <gtest/gtest.h>

#include <iostream>

using namespace reactive;

TEST(signalTrack, construct)
{
    btl::collection<int> c;

    {
        btl::collection<int> c2;

        auto w = c2.writer();
        w.push_back(0);
        w.push_back(1);
        w.push_back(2);
        w.push_back(3);
        w.push_back(4);
        w.push_back(5);
        c = std::move(c2);
    }

    auto s = signal::track(c);

    EXPECT_FALSE(s.hasChanged());

    {
        auto w = c.writer();
        w.push_back(6);
    }

    EXPECT_FALSE(s.hasChanged());

    s.beginTransaction();
    s.endTransaction(std::chrono::milliseconds(0));

    EXPECT_TRUE(s.hasChanged());

    int i = 0;
    for (auto const& v : s.evaluate())
    {
        EXPECT_EQ(i, v);
        ++i;
    }

    s.beginTransaction();
    s.endTransaction(std::chrono::milliseconds(0));

    EXPECT_FALSE(s.hasChanged());

}

/*TEST(signalTrack, mapped)
{
    btl::collection<int> c;

    {
        btl::collection<int> c2;

        auto w = c2.writer();
        w.push_back(0);
        w.push_back(1);
        w.push_back(2);
        w.push_back(3);
        w.push_back(4);
        w.push_back(5);
        c = std::move(c2);
    }

    auto c3 = btl::map_collection([](int, size_t i) -> int
            {
                return i;
            }, c);

    auto s1 = signal::track(c3);

    auto s2 = signal::map([](std::vector<int> const& values)
            -> std::vector<int>
            {
                std::cout << values.size() << std::endl;
                return values;
            }, s1);


}*/

