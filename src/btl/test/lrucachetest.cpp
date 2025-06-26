#include "gtest/gtest.h"

#include <btl/lrucache.h>

#include <iostream>

TEST(LruCache, Insertion)
{
    btl::LruCache<std::string, int> cache(10);

    cache.insert(std::make_pair("One", 1));
    cache.insert(std::make_pair("Two", 2));
    cache.insert(std::make_pair("Three", 3));
    cache.insert(std::make_pair("Four", 4));
    cache.insert(std::make_pair("Five", 5));
    cache.insert(std::make_pair("Six", 6));
    cache.insert(std::make_pair("Seven", 7));
    cache.insert(std::make_pair("Eight", 8));
    cache.insert(std::make_pair("Nine", 9));
    cache.insert(std::make_pair("Ten", 10));

    EXPECT_EQ(10, cache.size());

    cache.insert(std::make_pair("Eleven", 11));

    // Cache size shouldn't exceed 10
    EXPECT_EQ(10, cache.size());

    EXPECT_EQ(cache.end(), cache.find("One"));

    // This should move "Two" up in lru
    EXPECT_NE(cache.end(), cache.find("Two"));

    cache.insert(std::make_pair("Twelve", 12));

    // Two should still be in cache
    EXPECT_NE(cache.end(), cache.find("Two"));

    cache.insert(std::make_pair("Three", 30));
    EXPECT_EQ(30, cache.find("Three")->second);
}

