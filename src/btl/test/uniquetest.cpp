#include <btl/unique.h>

#include "gtest/gtest.h"

#include <string>

using namespace btl;

TEST(unique, construct)
{
    unique<std::string> s = make_unique<std::string>("test");
    EXPECT_EQ(std::string("test"), *s);

    std::unique_ptr<std::string> p(new std::string("test"));
    unique<std::string> s2(std::move(p));

    EXPECT_EQ(std::string("test"), *s2);
}

TEST(unique, dereference)
{
    unique<std::string> s = make_unique<std::string>("test");
    EXPECT_EQ(4, s->size());

    auto const& s2 = s;
    EXPECT_EQ(4, s2->size());

    EXPECT_EQ(std::string("test"), *s2);
}

