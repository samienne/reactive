#include "gtest/gtest.h"

#include <avg/transform.h>
#include <ase/vector.h>

#include <iostream>
#include <utility>

using namespace avg;

float const pi = 3.1415927f;
float const epsilon = 0.0001f;

bool equals(avg::Vector2f v1, avg::Vector2f v2)
{
    return std::abs(v1[0] - v2[0]) < 0.0001f
        && std::abs(v1[1] - v2[1]) < 0.0001f;
}

TEST(Transform, Identity)
{
    avg::Transform t1;

    EXPECT_EQ(1.0f, t1.getScale());
    EXPECT_EQ(0.0f, t1.getRotation());
    EXPECT_EQ(avg::Vector2f(0.0f, 0.0f), t1.getTranslation());
}

TEST(Transform, Inverse)
{
    auto t1 = avg::Transform()
        .setScale(10.0f)
        .setTranslation(avg::Vector2f(20.0f, -12.0f));

    avg::Transform t2 = t1.inverse();
    avg::Transform t3 = t2 * t1;
    avg::Transform identity;

    EXPECT_EQ(identity, t3);

    avg::Vector2f v1(10.f, 3.0f);
    avg::Vector2f v2 = t1 * v1;
    avg::Vector2f v3 = t2 * v2;

    EXPECT_EQ(true, equals(v1, v3));
}

TEST(Transform, Rotation)
{
    auto t1 = avg::Transform()
        .setRotation(3.1415927f / 2.0f); // 90 degrees

    avg::Vector2f v(1.0, 0.0);
    avg::Vector2f v2 = t1 * v;
    avg::Vector2f v3(0.0, 1.0);

    EXPECT_EQ(true, equals(v2, v3));

    auto t2 = avg::Transform()
        .rotate(3.1415927f / 2.0f) // 90 degrees
        .rotate(3.1415927f / 2.0f) // 90 degrees
        .rotate(3.1415927f / 2.0f) // 90 degrees
        .rotate(3.1415927f / 2.0f); // 90 degrees
    avg::Transform identity;

    //std::cout << "t2.getRotation() = " << t2.getRotation() << std::endl;
    EXPECT_EQ(identity, t2);
}

TEST(Transform, Translation)
{
    auto t1 = avg::Transform()
        .setTranslation(avg::Vector2f(10.0f, 2.0f));
    avg::Vector2f v(8.0, 5.0);
    avg::Vector2f v2 = t1 * v;
    avg::Vector2f v3(18.0f, 7.0f);
    EXPECT_EQ(true, equals(v2, v3));
}

TEST(Transform, Operators)
{
    auto t1 = avg::Transform()
        .setTranslation(avg::Vector2f(10.0f, 2.0f))
        .setRotation(3.1415927f / 2.0f) // 90 degrees
        .setTranslation(avg::Vector2f(20.0f, -12.0f));

    auto t2 = avg::Transform()
        .setTranslation(avg::Vector2f(10.0f, 2.0f))
        .rotate(3.1415927f / 2.0f) // 90 degrees
        .rotate(3.1415927f / 2.0f) // 90 degrees
        .setTranslation(avg::Vector2f(20.0f, -12.0f));
}

TEST(Transform, RotateTranslate)
{
    int n = 32;

    float step = (2.0f * pi) / (float)n;

    Vector2f v(10.0f, 5.0f);

    for (int i = 0; i < n; ++i)
    {
        float a = (float)i * step;

        auto t = avg::Transform()
            .rotateAround(Vector2f(5.0f, 5.0f), a)
            ;

        auto v2 = t * v;
        Vector2f v3 = v2 - Vector2f(5.0f, 5.0f);

        float d = std::sqrt(v3.x() * v3.x() + v3.y() * v3.y());

        EXPECT_NEAR(5.0f, d, epsilon);
    }
}

TEST(Transform, ComposeRotateAround)
{
    auto t1 = Transform().translate(10.0f, 0.0f);
    auto t2 = Transform().rotate(pi);
    auto t3 = Transform().translate(-10.0f, 0.0f);

    auto t4 = t1 * t2 * t3;

    auto v = Vector2f(15.0, 0.0f);

    auto v1 = t1 * (t2 * (t3 * v));
    auto v2 = t4 * v;

    EXPECT_NEAR(v1.x(), v2.x(), epsilon);
    EXPECT_NEAR(v1.y(), v2.y(), epsilon);
}

