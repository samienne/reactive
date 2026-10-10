#include <arrange/errors.h>
#include <arrange/id.h>
#include <arrange/variable.h>

#include <gtest/gtest.h>

#include <unordered_set>

namespace arrange
{

namespace
{

constexpr Id kHighBit = Id{1} << 63;

}  // namespace

TEST(Variable, DefaultConstructedHasNonNullAutoId)
{
    Variable v;
    EXPECT_NE(v.id(), nullId);
    EXPECT_NE(v.id() & kHighBit, Id{0}) << "auto id must have high bit set";
    EXPECT_TRUE(v.name().empty());
}

TEST(Variable, DistinctDefaultsHaveDistinctIds)
{
    Variable a;
    Variable b;
    EXPECT_NE(a.id(), b.id());
}

TEST(Variable, NamedConstructorStoresName)
{
    Variable v{"left"};
    EXPECT_EQ(v.name(), "left");
    EXPECT_NE(v.id() & kHighBit, Id{0});
}

TEST(Variable, UserIdConstructorAcceptsLower63Bits)
{
    Variable v{Id{42}};
    EXPECT_EQ(v.id(), Id{42});
    EXPECT_TRUE(v.name().empty());
}

TEST(Variable, UserIdConstructorAcceptsNameToo)
{
    Variable v{Id{42}, "width"};
    EXPECT_EQ(v.id(), Id{42});
    EXPECT_EQ(v.name(), "width");
}

TEST(Variable, UserIdRejectsNullId)
{
    EXPECT_THROW(Variable{nullId}, BadId);
    EXPECT_THROW((Variable{nullId, "x"}), BadId);
}

TEST(Variable, UserIdRejectsHighBitSet)
{
    EXPECT_THROW(Variable{kHighBit}, BadId);
    EXPECT_THROW(Variable{kHighBit | Id{5}}, BadId);
}

TEST(Variable, UserIdAcceptsMaxLower63)
{
    const Id max = kHighBit - 1;
    Variable v{max};
    EXPECT_EQ(v.id(), max);
}

TEST(Variable, CopyPreservesIdentity)
{
    Variable a{Id{7}, "seven"};
    Variable b = a;
    EXPECT_EQ(a.id(), b.id());
    EXPECT_EQ(a.name(), b.name());
    EXPECT_EQ(a, b);
}

TEST(Variable, EqualityIsIdBased)
{
    Variable a{Id{7}, "one_name"};
    Variable b{Id{7}, "another_name"};
    EXPECT_EQ(a, b) << "names must not affect equality";

    Variable c{Id{8}, "one_name"};
    EXPECT_NE(a, c);
}

TEST(Variable, HashIsIdBased)
{
    Variable a{Id{7}};
    Variable b{Id{7}, "name"};
    VariableHash h;
    EXPECT_EQ(h(a), h(b));
}

TEST(Variable, UsableInUnorderedSet)
{
    std::unordered_set<Variable, VariableHash> s;
    Variable a{Id{1}};
    Variable b{Id{2}};
    s.insert(a);
    s.insert(b);
    s.insert(Variable{Id{1}, "dup"});  // same id as a — must not duplicate
    EXPECT_EQ(s.size(), 2u);
}

}  // namespace arrange
