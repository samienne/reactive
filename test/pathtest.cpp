#include <avg/path.h>
#include <avg/pathbuilder.h>
#include <avg/obb.h>

#include <ase/vector.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <iostream>
#include <utility>

using namespace avg;

TEST(Path, Construct)
{
    auto path1 = avg::PathBuilder(pmr::new_delete_resource())
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1))
        .build();

    EXPECT_FALSE(path1.isEmpty());

    avg::Path path2(pmr::new_delete_resource());

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
    auto path1 = avg::PathBuilder(pmr::new_delete_resource())
        .lineTo(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1))
        .build();

    avg::Path path2(pmr::new_delete_resource());
    path2 = path1;

    EXPECT_TRUE(path1 == path2);
    EXPECT_FALSE(path1 != path2);
}

TEST(Path, Addition)
{
    auto pathBuilder = avg::PathBuilder(pmr::new_delete_resource())
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1));

    auto pathBuilder2 = avg::PathBuilder(pmr::new_delete_resource())
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.1, 0.2))
        .lineTo(ase::Vector2f(0.4, 0.1));

    auto pathBuilder3 = avg::PathBuilder(pmr::new_delete_resource())
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1))
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.1, 0.2))
        .lineTo(ase::Vector2f(0.4, 0.1));

    avg::Path path1 = pathBuilder.build();
    avg::Path path2 = std::move(pathBuilder2).build();
    avg::Path path3 = std::move(pathBuilder3).build();
    avg::Path path4 = std::move(pathBuilder).build();

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
    auto path1 = avg::PathBuilder(pmr::new_delete_resource())
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1))
        .build();

    auto path2 = avg::PathBuilder(pmr::new_delete_resource())
        .start(ase::Vector2f(0.4, 0.2))
        .lineTo(ase::Vector2f(0.8, 0.6))
        .lineTo(ase::Vector2f(0.8, 0.2))
        .build();

    avg::Path path3 = path1 * 2.0;
    avg::Path path4 = path2 * 0.5;

    EXPECT_TRUE(path3 == path2);
    EXPECT_FALSE(path3 != path2);

    EXPECT_TRUE(path1 == path4);
    EXPECT_FALSE(path1 != path4);
}

TEST(Path, Offsetting)
{
    auto path1 = avg::PathBuilder(pmr::new_delete_resource())
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1))
        .build();

    auto path2 = avg::PathBuilder(pmr::new_delete_resource())
        .start(ase::Vector2f(0.4, 0.2))
        .lineTo(ase::Vector2f(0.6, 0.4))
        .lineTo(ase::Vector2f(0.6, 0.2))
        .build();

    avg::Path path3(pmr::new_delete_resource());

    auto t = avg::Transform()
        .translate(ase::Vector2f(0.2, 0.1));
    path3 = t * path1;

    EXPECT_TRUE(path3 == path2);
    EXPECT_FALSE(path3 != path2);
}

TEST(Path, SpecAssignToSelfModified)
{
    auto pathBuilder = avg::PathBuilder(pmr::new_delete_resource());

    pathBuilder = std::move(pathBuilder)
        .start(ase::Vector2f(0.2, 0.1))
        .lineTo(ase::Vector2f(0.4, 0.3))
        .lineTo(ase::Vector2f(0.4, 0.1));

    avg::Path p = pathBuilder.build();
    EXPECT_FALSE(p.isEmpty());
}

TEST(Path, emptyPathShouldHaveEmptyBoundingBox)
{
    avg::Path p(pmr::new_delete_resource());

    EXPECT_TRUE(p.getControlBb().isEmpty());
}

TEST(Path, aPathShouldHaveValidBoundingBox)
{
    avg::Path p = avg::PathBuilder(pmr::new_delete_resource())
            .start(avg::Vector2f(5.0f, 7.0f))
            .lineTo(avg::Vector2f(10.0f, 12.0f))
            .close()
            .build();

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

    avg::Path p = avg::PathBuilder(pmr::new_delete_resource())
            .start(avg::Vector2f(5.0f, 7.0f))
            .lineTo(avg::Vector2f(10.0f, 12.0f))
            .close()
            .build();

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

