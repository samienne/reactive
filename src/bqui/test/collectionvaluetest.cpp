#include <bqui/collection.h>

#include <gtest/gtest.h>

using namespace bqui;

TEST(collectionValue, copyConstructDeepCopies)
{
    CollectionValue<int> source(10);
    CollectionValue<int> copy(source);

    EXPECT_EQ(10, *copy);

    // Independent heap object.
    EXPECT_NE(source.ptr(), copy.ptr());

    // Mutating the copy does not affect the source.
    *copy = 20;
    EXPECT_EQ(20, *copy);
    EXPECT_EQ(10, *source);

    // Mutating the source does not affect the copy.
    *source = 30;
    EXPECT_EQ(30, *source);
    EXPECT_EQ(20, *copy);
}

TEST(collectionValue, copyAssignDeepCopies)
{
    CollectionValue<int> source(10);
    CollectionValue<int> target(99);

    target = source;

    EXPECT_EQ(10, *target);

    // Independent heap object.
    EXPECT_NE(source.ptr(), target.ptr());

    // Mutating the target does not affect the source.
    *target = 20;
    EXPECT_EQ(20, *target);
    EXPECT_EQ(10, *source);

    // Mutating the source does not affect the target.
    *source = 30;
    EXPECT_EQ(30, *source);
    EXPECT_EQ(20, *target);
}
