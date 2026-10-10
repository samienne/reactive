#include <arrange/constraint.h>
#include <arrange/diff.h>
#include <arrange/expression.h>
#include <arrange/id.h>
#include <arrange/strength.h>
#include <arrange/variable.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace arrange
{

namespace
{

Constraint makeC(Id id, double constant = 0.0)
{
    Variable v{Id{1}, "v"};
    return (Expression{v} == constant).withId(id);
}

bool containsId(const std::vector<Constraint>& v, Id id)
{
    return std::any_of(v.begin(), v.end(), [id](const Constraint& c) { return c.id() == id; });
}

std::size_t countId(const std::vector<Constraint>& v, Id id)
{
    return static_cast<std::size_t>(
        std::count_if(v.begin(), v.end(), [id](const Constraint& c) { return c.id() == id; }));
}

}  // namespace

TEST(Diff, EmptyVsEmpty)
{
    auto d = diffConstraints({}, {});
    EXPECT_TRUE(d.added.empty());
    EXPECT_TRUE(d.removed.empty());
}

TEST(Diff, EmptyBeforeAllAdded)
{
    std::vector<Constraint> after{makeC(Id{1}), makeC(Id{2})};
    auto d = diffConstraints({}, after);
    EXPECT_EQ(d.added.size(), 2u);
    EXPECT_TRUE(d.removed.empty());
    EXPECT_TRUE(containsId(d.added, Id{1}));
    EXPECT_TRUE(containsId(d.added, Id{2}));
}

TEST(Diff, EmptyAfterAllRemoved)
{
    std::vector<Constraint> before{makeC(Id{1}), makeC(Id{2})};
    auto d = diffConstraints(before, {});
    EXPECT_EQ(d.removed.size(), 2u);
    EXPECT_TRUE(d.added.empty());
}

TEST(Diff, IdenticalSetNoChanges)
{
    std::vector<Constraint> before{makeC(Id{1}), makeC(Id{2})};
    std::vector<Constraint> after{makeC(Id{1}), makeC(Id{2})};
    auto d = diffConstraints(before, after);
    EXPECT_TRUE(d.added.empty());
    EXPECT_TRUE(d.removed.empty());
}

TEST(Diff, AddRemoveMix)
{
    std::vector<Constraint> before{makeC(Id{1}), makeC(Id{2}), makeC(Id{3})};
    std::vector<Constraint> after{makeC(Id{2}), makeC(Id{3}), makeC(Id{4})};
    auto d = diffConstraints(before, after);

    ASSERT_EQ(d.added.size(), 1u);
    EXPECT_EQ(d.added[0].id(), Id{4});

    ASSERT_EQ(d.removed.size(), 1u);
    EXPECT_EQ(d.removed[0].id(), Id{1});
}

TEST(Diff, MatchingIdWithDifferentContentStillSkipped)
{
    // Per spec: matching is strictly by id. If the user reuses an id for
    // different content, the diff reports no change. The onus is on the
    // user to pick a new id when content changes.
    Variable v{Id{1}, "v"};
    Constraint before = (Expression{v} == 5.0).withId(Id{42});
    Constraint after = (Expression{v} == 99.0).withId(Id{42});

    auto d = diffConstraints({before}, {after});
    EXPECT_TRUE(d.added.empty());
    EXPECT_TRUE(d.removed.empty());
}

TEST(Diff, NullIdEntriesAlwaysAppearInDiff)
{
    std::vector<Constraint> before{makeC(nullId), makeC(nullId)};
    std::vector<Constraint> after{makeC(nullId), makeC(nullId), makeC(nullId)};

    auto d = diffConstraints(before, after);
    EXPECT_EQ(d.removed.size(), 2u) << "all nullId befores must be removed";
    EXPECT_EQ(d.added.size(), 3u) << "all nullId afters must be added";
}

TEST(Diff, MixOfNullAndIdentifiedEntries)
{
    std::vector<Constraint> before{makeC(Id{1}), makeC(nullId), makeC(Id{2})};
    std::vector<Constraint> after{makeC(Id{1}), makeC(nullId), makeC(Id{3})};
    auto d = diffConstraints(before, after);

    // Removed: nullId from before (always), Id{2} (dropped)
    EXPECT_EQ(d.removed.size(), 2u);
    EXPECT_EQ(countId(d.removed, nullId), 1u);
    EXPECT_EQ(countId(d.removed, Id{2}), 1u);
    EXPECT_EQ(countId(d.removed, Id{1}), 0u);

    // Added: nullId from after (always), Id{3} (new)
    EXPECT_EQ(d.added.size(), 2u);
    EXPECT_EQ(countId(d.added, nullId), 1u);
    EXPECT_EQ(countId(d.added, Id{3}), 1u);
    EXPECT_EQ(countId(d.added, Id{1}), 0u);
}

TEST(Diff, OrderIsReversedWhenInputsReversed)
{
    std::vector<Constraint> a{makeC(Id{1}), makeC(Id{2})};
    std::vector<Constraint> b{makeC(Id{2}), makeC(Id{3})};

    auto d1 = diffConstraints(a, b);
    auto d2 = diffConstraints(b, a);

    EXPECT_EQ(d1.added.size(), d2.removed.size());
    EXPECT_EQ(d1.removed.size(), d2.added.size());
    EXPECT_EQ(d1.added[0].id(), d2.removed[0].id());
    EXPECT_EQ(d1.removed[0].id(), d2.added[0].id());
}

TEST(Diff, DuplicateIdsInBefore)
{
    // Two befores with same id; after is empty. Both should be reported
    // as removed (the set-membership test doesn't dedupe the output).
    std::vector<Constraint> before{makeC(Id{5}), makeC(Id{5})};
    auto d = diffConstraints(before, {});
    EXPECT_EQ(d.removed.size(), 2u);
}

}  // namespace arrange
