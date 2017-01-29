#include <btl/tupleswitch.h>
#include <btl/tuplereverse.h>
#include <btl/tuplehead.h>
#include <btl/tupletail.h>
#include <btl/tupleallbutlast.h>
#include <btl/tuplelast.h>

#include <btl/moveonlyfunction.h>

#include <memory>

#include "gtest/gtest.h"


static_assert(std::is_same<std::tuple<int, std::string, char>,
        decltype(btl::tuple_reverse(std::declval<std::tuple<char,
                std::string, int>>()))>::value, "");

TEST(Tuple, reverse)
{
    auto t1 = std::tuple<int, btl::MoveOnlyFunction<void()>>(10, [](){});

    auto t2 = btl::tuple_reverse(std::move(t1));

    EXPECT_EQ(10, std::get<1>(t2));
}

TEST(Tuple, head)
{
    auto t1 = std::tuple<int, btl::MoveOnlyFunction<void()>>(10, [](){});

    static_assert(std::is_same<
            int&&,
            decltype(tuple_head(std::move(t1)))
            >::value, "");

    static_assert(std::is_same<
            int&,
            decltype(tuple_head(t1))
            >::value, "");

    static_assert(std::is_same<
            int const&,
            std::decay_t<decltype(tuple_head(t1))> const&
            >::value, "");

    EXPECT_EQ(10, tuple_head(t1));
}

TEST(Tuple, tail)
{
    auto t1 = std::tuple<int, std::string, std::unique_ptr<int>>(
            10, "test", nullptr);

    auto t2 = btl::tuple_tail(std::move(t1));

    EXPECT_EQ(std::string("test"), std::get<0>(t2));
}

TEST(Tuple, all_but_last)
{
    auto t1 = std::tuple<int, std::string, std::unique_ptr<int>>(
            10, "test", nullptr);

    auto t2 = btl::tuple_all_but_last(std::move(t1));

    auto const& t3 = t1;

    EXPECT_EQ(std::make_tuple(10, std::string("test")), t2);

    static_assert(std::is_same<
            std::tuple<int, std::string>,
            decltype(btl::tuple_all_but_last(t1))
            >::value, "");

    static_assert(std::is_same<
            std::tuple<int, std::string>,
            decltype(btl::tuple_all_but_last(std::move(t1)))
            >::value, "");

    static_assert(std::is_same<
            std::tuple<int, std::string>,
            decltype(btl::tuple_all_but_last(t3))
            >::value, "");

    auto t4 = std::forward_as_tuple(20, std::string("test"));

    static_assert(std::is_same<
            std::tuple<int&&>,
            decltype(btl::tuple_all_but_last(t4))
            >::value, "");

    static_assert(std::is_same<
            std::tuple<int&&>,
            decltype(btl::tuple_all_but_last(std::move(t4)))
            >::value, "");
}

TEST(Tuple, last)
{
    auto t1 = std::tuple<int, std::string, std::unique_ptr<int>>(
            10, "test", nullptr);

    static_assert(std::is_same<
            std::unique_ptr<int>,
            decltype(btl::tuple_last(t1))
            >::value, "");

    static_assert(std::is_same<
            std::unique_ptr<int>,
            decltype(btl::tuple_last(std::move(t1)))
            >::value, "");

    static_assert(std::is_same<
            std::unique_ptr<int> const&,
            std::decay_t<decltype(btl::tuple_last(t1))> const&
            >::value, "");

    auto n = btl::tuple_last(std::move(t1));
}

TEST(Tuple, tuple_switch)
{
    std::string str("test");
    auto t1 = std::tuple<int, std::string const&, std::unique_ptr<int>>(
            10, str, nullptr);
    auto t2 = btl::tuple_switch(std::move(t1));

    EXPECT_EQ(std::string("test"), std::get<2>(t2));

    static_assert(std::is_same<
            std::tuple<std::unique_ptr<int>, int, std::string const&>,
            std::decay_t<decltype(btl::tuple_switch(t1))>
            >::value, "");
}

