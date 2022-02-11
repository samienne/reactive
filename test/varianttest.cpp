#include <btl/variant.h>
#include <btl/fnv1a.h>
#include <btl/uhash.h>
#include <btl/hash.h>

#include <avg/transform.h>
#include <avg/shape.h>

#include <gtest/gtest.h>

#include <iostream>
#include <utility>
#include <string>
#include <vector>

using namespace avg;

struct S
{
    int i;
    char j;
    uint64_t k;
};

TEST(variant, maxSizeof)
{
    btl::multiply_t<avg::Transform, std::optional<avg::Pen>> n;
    size_t s = sizeof(S);

    auto s2 = (btl::detail::MaxSizeOf<char, uint32_t, S>::value);
    EXPECT_EQ(s, s2);

    auto s3 = (btl::detail::MaxSizeOf<char, uint32_t, int64_t>::value);
    EXPECT_EQ(8, s3);
}

TEST(variant, isAnyOf)
{
    bool b = btl::IsOneOf<int, char, int>::value;
    EXPECT_TRUE(b);

    bool b2 = btl::IsOneOf<int, char, bool>::value;
    EXPECT_FALSE(b2);

    bool b3 = btl::IsOneOf<S, char, S>::value;
    EXPECT_TRUE(b3);

    bool b4 = btl::IsOneOf<int,
         btl::variant<int, bool>>::value;
    EXPECT_FALSE(b4);
}

TEST(variant, isAssignableTo)
{
    bool b = btl::detail::IsAssignableTo<
        btl::variant<std::string, char, bool, int, uint64_t>,
        btl::variant<int, char>>::value;
    EXPECT_TRUE(b);

    bool b2 = btl::detail::IsAssignableTo<
        btl::variant<std::string, char, bool, uint64_t>,
        btl::variant<int, char, int64_t>>::value;
    EXPECT_FALSE(b2);

    bool b3 = btl::detail::IsAssignableTo<
        btl::variant<std::string, uint64_t>,
        btl::variant<std::string, uint64_t>>::value;
    EXPECT_TRUE(b3);
}

TEST(variant, construct)
{
    btl::variant<int> u(10);
    EXPECT_TRUE(u.is<int>());
    EXPECT_FALSE(u.is<char>());
}

TEST(variant, copyConstruct)
{
    btl::variant<int, std::string> u(std::string("test"));

    decltype(u) u2(u);
    EXPECT_EQ(std::string("test"), u2.get<std::string>());

    btl::variant<std::string> u3(std::string("testtest"));
    u = u3;

    EXPECT_EQ(std::string("testtest"), u.get<std::string>());

    btl::variant<std::string, std::vector<std::string>> u4(
            std::string("test"));

    auto v = std::vector<std::string>{"test1", "test2"};
    u4 = v;
    EXPECT_EQ(v, u4.get<std::vector<std::string>>());
    auto test = std::string("test");
    u4 = test;
    EXPECT_EQ(std::string("test"), u4.get<std::string>());

    btl::variant<std::string, std::vector<std::string>> u5(
            std::string("test"));
    u5 = u4;
}

TEST(variant, moveConstruct)
{
    btl::variant<int, std::string, char> u(std::string("test"));

    decltype(u) u2 = std::move(u);
    EXPECT_EQ(std::string("test"), u2.get<std::string>());

    btl::variant<int, std::string> u3(std::string("testtest"));
    u = u3;
    EXPECT_EQ(std::string("testtest"), u.get<std::string>());

    btl::variant<char, int, std::string> u4(u);
    EXPECT_EQ(std::string("testtest"), u4.get<std::string>());

    btl::variant<char, int, std::string> u5(std::move(u));
    EXPECT_EQ(std::string("testtest"), u5.get<std::string>());

    btl::variant<std::string, std::vector<std::string>> u6(
            std::string("test"));
    auto v = std::vector<std::string>{"test1", "test2"};
    auto v2 = v;
    u6 = std::move(v);
    EXPECT_EQ(v2, u6.get<std::vector<std::string>>());
    u6 = std::string("test");
    EXPECT_EQ(std::string("test"), u6.get<std::string>());
}

TEST(variant, get)
{
    btl::variant<int> u(10);
    EXPECT_EQ(10, u.get<int>());
}

TEST(variant, copyAssign)
{
    btl::variant<int, std::string, char> u(10);

    decltype(u) u2(std::string("test"));
    u = u2;

    EXPECT_EQ(std::string("test"), u.get<std::string>());

    btl::variant<std::string, char> u3(std::string("testtest"));
    u = u3;
    EXPECT_EQ(std::string("testtest"), u.get<std::string>());
}

TEST(variant, moveAssign)
{
    btl::variant<int, std::string, char> u(10);

    decltype(u) u2(std::string("test"));
    u = std::move(u2);

    EXPECT_EQ(std::string("test"), u.get<std::string>());

    btl::variant<std::string, char> u3(std::string("testtest"));
    u = u3;

    EXPECT_EQ(std::string("testtest"), u.get<std::string>());
}

TEST(variant, assign)
{
    btl::variant<int, std::string, char> u(std::string("aijai"));
    u = 10;

    EXPECT_TRUE(u.is<int>());
    EXPECT_EQ(10, u.get<int>());

    btl::variant<int, std::string, char> u2(20);
    u2 = u;
    EXPECT_EQ(10, u2.get<int>());

    btl::variant<char, std::string, int> u3(30);
    u3 = std::move(u);
    EXPECT_EQ(10, u3.get<int>());
}

TEST(variant, match)
{
    btl::variant<int, std::string> u(std::string("aijai"));

    std::type_index t = typeid(void);

    u.match<std::string>([&](std::string& s){ t = typeid(s); })
        .match<int>([&](int& i) { t = typeid(i); } );

    std::type_index e = typeid(std::string);
    EXPECT_EQ(e, t);
}

TEST(variant, compareEqual)
{
    btl::variant<int, std::string, char> u(std::string("testtest"));
    btl::variant<int, std::string, char> u2(std::string("testtest"));
    btl::variant<std::string> u3(std::string("testtest"));
    btl::variant<int, std::string, char> u4(30);
    btl::variant<int, std::string> u5(30);

    EXPECT_TRUE(u == u2);

    std::string s("testtest");

    EXPECT_TRUE(u == s);

    std::string s2("notequal");
    EXPECT_FALSE(u == s2);

    EXPECT_TRUE(u3 == u);

    EXPECT_FALSE(u == 20);

    EXPECT_FALSE(u == u4);

    EXPECT_FALSE(u == u5);
}

TEST(variant, add)
{
    btl::variant<int, uint64_t> u(10);
    u = u + 10;

    EXPECT_EQ(20, u.get<int>());
}

TEST(variant, multiply)
{
    btl::variant<int, uint64_t> u(10);
    u = u * 10;

    EXPECT_EQ(100, u.get<int>());
}

TEST(variant, hash)
{
    btl::variant<int, std::string> u(std::string("test"));

    btl::uhash<btl::fnv1a> h;
    auto h1 = h(u);
    auto h2 = h(std::string("test"));

    EXPECT_EQ(h1, h2);
}

