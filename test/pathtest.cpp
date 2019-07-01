#include <avg/path.h>
#include <avg/pathbuilder.h>
#include <avg/obb.h>

#include <ase/vector.h>

#include <gtest/gtest.h>

#include <iostream>
#include <utility>

using namespace avg;

TEST(Path, Construct)
{
    auto pathSpec = avg::PathBuilder()
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1));

    avg::Path path1(std::move(pathSpec));

    EXPECT_FALSE(path1.isEmpty());

    avg::Path path2;

    EXPECT_TRUE(path2.isEmpty());

    path2 = path1;

    EXPECT_FALSE(path2.isEmpty());

    EXPECT_TRUE(path1 == path2);
    EXPECT_FALSE(path1 != path2);

    avg::Path path3(std::move(path2));

    EXPECT_TRUE(path1 == path3);
    EXPECT_FALSE(path1 != path3);
}

TEST(Path, Assignment)
{
    auto pathSpec = avg::PathBuilder()
        .lineTo(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1));

    avg::Path path1(std::move(pathSpec));

    avg::Path path2;
    path2 = path1;

    EXPECT_TRUE(path1 == path2);
    EXPECT_FALSE(path1 != path2);
}

TEST(Path, Addition)
{
    auto pathSpec = avg::PathBuilder()
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1));

    auto pathSpec2 = avg::PathBuilder()
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.1, 0.2))
        .lineTo(ase::Vector2f(0.4, 0.1));

    auto pathSpec3 = avg::PathBuilder()
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1))
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.1, 0.2))
        .lineTo(ase::Vector2f(0.4, 0.1));

    avg::Path path1 = avg::PathBuilder(pathSpec);
    avg::Path path2(std::move(pathSpec2));
    avg::Path path3(std::move(pathSpec3));
    avg::Path path4(std::move(pathSpec));

    EXPECT_FALSE(path1 == path2);
    EXPECT_TRUE(path1 != path2);

    avg::Path path5 = path1 + path2;
    EXPECT_TRUE(path5 == path3);
    EXPECT_FALSE(path5 != path3);

    path4 += path2;
    EXPECT_TRUE(path4 == path3);
    EXPECT_FALSE(path4 != path3);
}

TEST(Path, Scaling)
{
    auto pathSpec = avg::PathBuilder()
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1));

    auto pathSpec2 = avg::PathBuilder()
        .start(ase::Vector2f(0.4, 0.2))
        .lineTo(ase::Vector2f(0.8, 0.6))
        .lineTo(ase::Vector2f(0.8, 0.2));

    avg::Path path1(std::move(pathSpec));
    avg::Path path2(std::move(pathSpec2));
    avg::Path path3 = path1 * 2.0;
    avg::Path path4 = path2 * 0.5;

    EXPECT_TRUE(path3 == path2);
    EXPECT_FALSE(path3 != path2);

    EXPECT_TRUE(path1 == path4);
    EXPECT_FALSE(path1 != path4);
}

TEST(Path, Offsetting)
{
    auto pathSpec = avg::PathBuilder()
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1));

    auto pathSpec2 = avg::PathBuilder()
        .start(ase::Vector2f(0.4, 0.2))
        .lineTo(ase::Vector2f(0.6, 0.4))
        .lineTo(ase::Vector2f(0.6, 0.2));

    avg::Path path1(std::move(pathSpec));
    avg::Path path2(std::move(pathSpec2));
    avg::Path path3;

    auto t = avg::Transform()
        .translate(ase::Vector2f(0.2, 0.1));
    path3 = t * path1;

    EXPECT_TRUE(path3 == path2);
    EXPECT_FALSE(path3 != path2);
}

TEST(Path, SpecAssignToSelfModified)
{
    auto pathSpec = avg::PathBuilder();

    pathSpec = std::move(pathSpec)
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1));

    avg::Path p(pathSpec);
    EXPECT_FALSE(p.isEmpty());
}

TEST(Path, emptyPathShouldHaveEmptyBoundingBox)
{
    avg::Path p;

    EXPECT_TRUE(p.getControlBb().isEmpty());
}

TEST(Path, aPathShouldHaveValidBoundingBox)
{
    avg::Path p(avg::PathBuilder()
            .start(avg::Vector2f(5.0f, 7.0f))
            .lineTo(avg::Vector2f(10.0f, 12.0f))
            .close()
            );

    auto r = p.getControlBb();

    EXPECT_FALSE(r.isEmpty());

    EXPECT_EQ(5.0f, r.getLeft());
    EXPECT_EQ(7.0f, r.getBottom());
    EXPECT_EQ(10.0f, r.getRight());
    EXPECT_EQ(12.0f, r.getTop());

}

TEST(Path, rotatedPathShouldHaveLargerBoundingBox)
{
    float const pi = 3.1415927f;

    avg::Path p(avg::PathBuilder()
            .start(avg::Vector2f(5.0f, 7.0f))
            .lineTo(avg::Vector2f(10.0f, 12.0f))
            .close()
            );

    auto r1 = p.getControlBb();

    auto t = avg::Transform()
        .rotateAround(r1.getCenter(), pi / 1.0f)
        ;

    auto p2 = t * p;

    auto r2 = p2.getControlBb();

    EXPECT_GE(r2.getRight(), r1.getRight());
    EXPECT_GE(r2.getTop(), r1.getTop());
    EXPECT_LE(r2.getLeft(), r1.getLeft());
    EXPECT_LE(r2.getBottom(), r1.getBottom());
}

