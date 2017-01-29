#include <btl/shared.h>

#include "gtest/gtest.h"

#include <string>

using namespace btl;

TEST(shared, construct)
{
    shared<std::string> s(std::make_shared<std::string>("test"));
    EXPECT_EQ(std::string("test"), *s);

    auto p = std::make_shared<std::string>("test");
    shared<std::string> s2(p);

    EXPECT_EQ(std::string("test"), *s2);

    EXPECT_EQ(p, s2.ptr());
}

TEST(shared, dereference)
{
    shared<std::string> s(std::make_shared<std::string>("test"));
    EXPECT_EQ(4, s->size());

    auto const& s2 = s;
    EXPECT_EQ(4, s2->size());

    EXPECT_EQ(std::string("test"), *s2);
}

