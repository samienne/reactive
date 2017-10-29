#include <reactive/signal/databind.h>
#include <reactive/signaltype.h>
#include <reactive/signal/map.h>
#include <reactive/signal/update.h>

#include <btl/collection.h>

#include <gtest/gtest.h>

#include <string>
#include <iostream>

using namespace reactive;

TEST(DataBindSignal, pushBackAndErase)
{
    btl::collection<int> c;

    {
        auto w = c.writer();
        w.push_back(1);
        w.push_back(2);
        w.push_back(3);
    }

    auto s1 = signal::dataBind(c, [](signal2::Signal<btl::option<int>> data,
                signal::IndexSignal)
            -> signal2::Signal<btl::option<std::string>>
            {
                return signal::map([](btl::option<int> data)
                        -> btl::option<std::string>
                    {
                        if (!data.valid())
                            return btl::none;

                        return btl::just("test" + std::to_string(*data));

                    }, std::move(data));
            });

    auto v1 = s1.evaluate();
    EXPECT_EQ(3, v1.size());
    EXPECT_EQ("test1", v1.at(0));
    EXPECT_EQ("test2", v1.at(1));
    EXPECT_EQ("test3", v1.at(2));

    {
        auto w = c.writer();
        w.push_back(8);
    }

    EXPECT_FALSE(s1.hasChanged());

    signal::update(s1, {1, signal_time_t(0)});

    EXPECT_TRUE(s1.hasChanged());

    v1 = s1.evaluate();
    EXPECT_EQ(4, v1.size());
    EXPECT_EQ("test1", v1.at(0));
    EXPECT_EQ("test2", v1.at(1));
    EXPECT_EQ("test3", v1.at(2));
    EXPECT_EQ("test8", v1.at(3));

    signal::update(s1, {2, signal_time_t(0)});

    EXPECT_FALSE(s1.hasChanged());

    {
        auto w = c.writer();
        w.erase(w.begin());
    }

    EXPECT_FALSE(s1.hasChanged());

    signal::update(s1, {3, signal_time_t(0)});

    EXPECT_TRUE(s1.hasChanged());

    v1 = s1.evaluate();
    EXPECT_EQ(3, v1.size());
    EXPECT_EQ("test2", v1.at(0));
    EXPECT_EQ("test3", v1.at(1));
    EXPECT_EQ("test8", v1.at(2));

    std::cout << "update" << std::endl;

    {
        auto w = c.writer();
        for (size_t i = 0u; i < w.size(); ++i)
            w[i] = w[i] + 1;
    }

    signal::update(s1, {4, signal_time_t(0)});

    EXPECT_TRUE(s1.hasChanged());
    v1 = s1.evaluate();
    EXPECT_EQ("test3", v1.at(0));
    EXPECT_EQ("test4", v1.at(1));
    EXPECT_EQ("test9", v1.at(2));

    {
        auto w = c.writer();
        w.erase(w.begin()+1);
        w.insert(w.begin(), 6);
        w.insert(w.begin(), 7);
    }

    signal::update(s1, {5, signal_time_t(0)});
    v1 = s1.evaluate();

    /*for (auto const& v : v1)
    {
        std::cout << v << std::endl;
    }*/

    EXPECT_EQ("test7", v1.at(0));
    EXPECT_EQ("test6", v1.at(1));
    EXPECT_EQ("test3", v1.at(2));
    //EXPECT_EQ("test4", v1.at(2));
    EXPECT_EQ("test9", v1.at(3));
}

TEST(DataBindSignal, keepErased)
{
    btl::collection<int> c;

    {
        auto w = c.writer();
        w.push_back(10);
        w.push_back(20);
        w.push_back(30);
    }

    auto keep = signal::input(true);

    auto s1 = signal::dataBind(c, [keep](signal2::Signal<btl::option<int>> data,
                signal::IndexSignal)
            -> signal2::Signal<btl::option<std::string>>
            {
                return signal::map([](btl::option<int> data, bool keep)
                        -> btl::option<std::string>
                    {
                        if (data.valid())
                            return btl::just("test" + std::to_string(*data));
                        else if (keep)
                            return btl::just(std::string("removed"));
                        else
                            return btl::none;
                    }, std::move(data), btl::clone(keep.signal));
            });


    signal::update(s1, {1, signal_time_t(0)});

    {
        auto w = c.writer();
        w.erase(w.begin());
        w.erase(w.begin());
    }

    signal::update(s1, {2, signal_time_t(0)});

    EXPECT_TRUE(s1.hasChanged());

    auto v1 = s1.evaluate();
    EXPECT_EQ(3, v1.size());

    EXPECT_EQ("removed", v1.at(0));
    EXPECT_EQ("removed", v1.at(1));
    EXPECT_EQ("test30", v1.at(2));

    keep.handle.set(false);

    signal::update(s1, {3, signal_time_t(0)});

    EXPECT_TRUE(s1.hasChanged());
    v1 = s1.evaluate();

    EXPECT_EQ(1, v1.size());

    EXPECT_EQ("test30", v1.at(0));
}

