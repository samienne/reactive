#include <btl/option.h>

#include <gtest/gtest.h>

#include <string>

using namespace btl;

TEST(option, construct)
{
    option<std::string> s;
    EXPECT_FALSE(s.valid());
    EXPECT_TRUE(s.empty());

    option<std::string> s2(btl::just("test"));
    EXPECT_EQ(std::string("test"), *s2);
    EXPECT_TRUE(s2.valid());

    std::string const str("testtest");

    option<std::string> s3(btl::just(str));
    EXPECT_EQ(std::string("testtest"), *s3);
}

TEST(option, assign)
{
    option<std::string> s;
    s = btl::just("test");

    EXPECT_EQ(std::string("test"), *s);

    s = none;
    EXPECT_TRUE(s.empty());

    std::string str("testtest");
    s = just(str);

    EXPECT_EQ(std::string("testtest"), *s);

    option<std::string> const& s2 = s;
    EXPECT_EQ(std::string("testtest"), *s2);

    EXPECT_EQ(std::string("testtest"), *std::move(s2));
}

TEST(option, dereference)
{
    option<std::string> s(just("test"));
    EXPECT_EQ(4, s->size());

    option<std::string> const s2(just("test"));
    EXPECT_EQ(4, s2->size());
}

TEST(option, compareEqual)
{
    option<std::string> s(just("test"));
    option<std::string> s2(just("test"));

    EXPECT_EQ(s, s2);

    option<std::string> s3;

    EXPECT_NE(s, s3);

    option<std::string> s4;
    EXPECT_EQ(s3, s4);
}

TEST(option, compareLess)
{
    option<std::string> s(just("test"));
    option<std::string> s2(just("test"));

    EXPECT_FALSE(s < s2);

    option<std::string> s3;

    EXPECT_TRUE(s3 < s);

    option<std::string> s4;
    EXPECT_FALSE(s3 < s4);
}

TEST(option, compareLessInt)
{
    option<int> s(just(1));
    option<int> s2(just(1));

    EXPECT_FALSE(s < s2);

    option<int> s3;

    EXPECT_TRUE(s3 < s);

    option<int> s4;
    EXPECT_FALSE(s3 < s4);
}

TEST(option, compareGreater)
{
    option<std::string> s(just("test"));
    option<std::string> s2(just("test"));

    EXPECT_FALSE(s > s2);

    option<std::string> s3;

    EXPECT_TRUE(s > s3);

    option<std::string> s4;
    EXPECT_FALSE(s3 > s4);
}

TEST(option, multiply)
{
    option<int> i(just(10));

    EXPECT_EQ(just(200), (20 * i));

    option<int> i2;

    i2 = 10 * i2;
    EXPECT_FALSE(i2.valid());

    i = i * 10;
    EXPECT_EQ(100, *i);

    i = i * option<int>(none);
    EXPECT_FALSE(i.valid());

    option<int> i3;
    i3 = i3 * 10;
    EXPECT_FALSE(i3.valid());

    option<int> i4(just(10));
    option<int> i5(just(20));
    i4 = i4 * i5;
    EXPECT_EQ(200, *i4);
}

TEST(option, multiplyAssign)
{
    option<int> i(just(10));
    i *= 20;

    EXPECT_EQ(200, *i);
}

TEST(option, hihgherOrder)
{
    option<option<int>> i;

    i = just(just(10));
}

