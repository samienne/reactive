#include <avg/path.h>
#include <avg/pathspec.h>
#include <ase/vector.h>

#include <gtest/gtest.h>

#include <iostream>
#include <utility>

TEST(Path, Construct)
{
    auto pathSpec = avg::PathSpec()
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
    auto pathSpec = avg::PathSpec()
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
    auto pathSpec = avg::PathSpec()
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1));

    auto pathSpec2 = avg::PathSpec()
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.1, 0.2))
        .lineTo(ase::Vector2f(0.4, 0.1));

    auto pathSpec3 = avg::PathSpec()
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1))
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.1, 0.2))
        .lineTo(ase::Vector2f(0.4, 0.1));

    avg::Path path1 = avg::PathSpec(pathSpec);
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
    auto pathSpec = avg::PathSpec()
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1));

    auto pathSpec2 = avg::PathSpec()
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
    auto pathSpec = avg::PathSpec()
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1));

    auto pathSpec2 = avg::PathSpec()
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
    auto pathSpec = avg::PathSpec();

    pathSpec = std::move(pathSpec)
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1));

    avg::Path p(pathSpec);
    EXPECT_FALSE(p.isEmpty());
}

