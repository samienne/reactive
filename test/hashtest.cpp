#include "btl/fnv1a.h"
#include "btl/uhash.h"
#include "btl/hash.h"

#include <avg/shape.h>
#include <avg/pen.h>
#include <avg/brush.h>
#include <avg/color.h>

#include <gtest/gtest.h>

#include <array>
#include <utility>
#include <string>
#include <iostream>

TEST(Hash, array)
{
    std::array<float, 4> a{{1.0f, 0.0f, 0.4f, 1.0f}};

    btl::uhash<btl::fnv1a> h;

    EXPECT_EQ(7469078562863869594u, h(a));
}

TEST(Hash, intTest)
{
    int i = 20;

    btl::uhash<btl::fnv1a> h;
    EXPECT_EQ(14764355265743743681u, h(i));
}

TEST(Hash, color)
{
    avg::Color color(1.0, 1.0, 1.0, 1.0);
    btl::uhash<btl::fnv1a> h;
    EXPECT_EQ(13403257112788641477u, h(color));

    btl::fnv1a::result_type r = h(avg::Color(1.0, 1.0, 1.0, 1.0));
    EXPECT_EQ(r, h(color));
}

TEST(Hash, brush)
{
    avg::Brush b(avg::Color(1.0, 1.0, 1.0, 1.0));

    btl::uhash<btl::fnv1a> h;
    EXPECT_EQ(13403257112788641477u, h(b));
}

TEST(Hash, pen)
{
    avg::Pen pen(avg::Brush(avg::Color(1.0f, 1.0f, 1.0f, 1.0f)), 2.0f);

    btl::uhash<btl::fnv1a> h;
    EXPECT_EQ(1790576748669079125u, h(pen));
}

TEST(Hash, vector)
{
    std::vector<int> v;
    v.push_back(10);
    v.push_back(20);

    btl::uhash<btl::fnv1a> h;
    EXPECT_TRUE(h(v) == 4430961869720737737u
            || h(v) == 3949976766425552089u);
}

TEST(Hash, path)
{
    auto s = avg::PathSpec()
        .start(avg::Vector2f(1.0f, 0.4f))
        .lineTo(avg::Vector2f(0.0f, 2.4f))
        .lineTo(avg::Vector2f(10.0f, 2.4f))
        .close();

    avg::Path p(s);

    btl::uhash<btl::fnv1a> h;
    EXPECT_TRUE(h(p) == 1520779240607667710u
            || h(p) == 9951566134402991406u);

    auto s2 = avg::PathSpec()
        .start(avg::Vector2f(1.0f, 0.4f))
        .lineTo(avg::Vector2f(0.0f, 2.4f))
        .lineTo(avg::Vector2f(10.0f, 2.4f))
        .close();

    avg::Path p2(s2);
    EXPECT_EQ(h(p), h(p2));
}

TEST(Hash, shape)
{
    auto p = avg::Path(avg::PathSpec()
        .start(avg::Vector2f(1.0f, 0.4f))
        .lineTo(avg::Vector2f(0.0f, 2.4f))
        .lineTo(avg::Vector2f(10.0f, 2.4f))
        .close());

    auto shape = avg::Shape()
        .setPath(p)
        .setBrush(btl::just(avg::Brush(avg::Color(1.0f, 0.0f, 0.0f, 1.0f))))
        .setPen(btl::just(avg::Pen(
                        avg::Brush(avg::Color(1.0f, 0.0f, 0.0f, 1.0f)), 3.0f)));

    btl::uhash<btl::fnv1a> h;
    EXPECT_TRUE(h(shape) == 3846001539217960958u
            || h(shape) == 5783354706658823854u);
}

