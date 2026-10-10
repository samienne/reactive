#include <arrange/constraint.h>
#include <arrange/expression.h>
#include <arrange/id.h>
#include <arrange/strength.h>
#include <arrange/variable.h>

#include <gtest/gtest.h>

namespace arrange
{

namespace
{

double coefficientOf(const Expression& e, const Variable& v)
{
    for (std::size_t i = 0; i < e.termCount(); ++i)
    {
        const auto t = e.term(i);
        if (t.variable == v)
        {
            return t.coefficient;
        }
    }
    return 0.0;
}

}  // namespace

TEST(Constraint, DefaultConstructedFromEqualityHasNullIdAndRequired)
{
    Variable a{Id{1}, "a"};
    Constraint c = Expression{a} == 5.0;
    EXPECT_EQ(c.id(), nullId);
    EXPECT_EQ(c.strength(), Strength::required());
}

TEST(Constraint, ExpressionIsLhsMinusRhs)
{
    Variable a{Id{1}, "a"};
    Variable b{Id{2}, "b"};
    // (a + 1) == (b + 3)  canonicalizes to a - b - 2  rel  0
    Constraint c = (Expression{a} + 1.0) == (Expression{b} + 3.0);
    const Expression& e = c.expression();
    EXPECT_DOUBLE_EQ(coefficientOf(e, a), 1.0);
    EXPECT_DOUBLE_EQ(coefficientOf(e, b), -1.0);
    EXPECT_DOUBLE_EQ(e.constant(), -2.0);
}

TEST(Constraint, LessEqualProducesConstraint)
{
    Variable a{Id{1}, "a"};
    Constraint c = Expression{a} <= 10.0;
    EXPECT_EQ(c.id(), nullId);
    EXPECT_EQ(c.strength(), Strength::required());
}

TEST(Constraint, GreaterEqualProducesConstraint)
{
    Variable a{Id{1}, "a"};
    Constraint c = Expression{a} >= 0.0;
    EXPECT_EQ(c.id(), nullId);
}

TEST(Constraint, WithIdSetsIdAndPreservesOthers)
{
    Variable a{Id{1}, "a"};
    Constraint c = (Expression{a} == 5.0) | Strength::strong();
    Constraint c2 = c.withId(Id{99});
    EXPECT_EQ(c2.id(), Id{99});
    EXPECT_EQ(c2.strength(), Strength::strong());
    EXPECT_EQ(c.id(), nullId) << "original must be untouched";
}

TEST(Constraint, WithIdNullIdClears)
{
    Variable a{Id{1}, "a"};
    Constraint c = (Expression{a} == 0.0).withId(Id{42});
    Constraint c2 = c.withId(nullId);
    EXPECT_EQ(c2.id(), nullId);
    EXPECT_EQ(c.id(), Id{42}) << "original must be untouched";
}

TEST(Constraint, WithStrengthChangesStrength)
{
    Variable a{Id{1}, "a"};
    Constraint c = Expression{a} == 5.0;
    Constraint c2 = c.withStrength(Strength::weak());
    EXPECT_EQ(c2.strength(), Strength::weak());
    EXPECT_EQ(c.strength(), Strength::required()) << "original must be untouched";
}

TEST(Constraint, WithStrengthPreservesId)
{
    Variable a{Id{1}, "a"};
    Constraint c = (Expression{a} == 5.0).withId(Id{7});
    Constraint c2 = c.withStrength(Strength::medium());
    EXPECT_EQ(c2.id(), Id{7});
    EXPECT_EQ(c2.strength(), Strength::medium());
}

TEST(Constraint, PipeStrengthAttaches)
{
    Variable a{Id{1}, "a"};
    Constraint c = (Expression{a} == 5.0) | Strength::strong();
    EXPECT_EQ(c.strength(), Strength::strong());
    EXPECT_EQ(c.id(), nullId);
}

TEST(Constraint, PipeIsSymmetric)
{
    Variable a{Id{1}, "a"};
    Constraint left = (Expression{a} == 5.0) | Strength::strong();
    Constraint right = Strength::strong() | (Expression{a} == 5.0);
    EXPECT_EQ(left.strength(), right.strength());
}

TEST(Constraint, PipeOnPrebuiltConstraintPreservesId)
{
    Variable a{Id{1}, "a"};
    Constraint c = (Expression{a} == 5.0).withId(Id{42});
    Constraint c2 = c | Strength::weak();
    EXPECT_EQ(c2.id(), Id{42});
    EXPECT_EQ(c2.strength(), Strength::weak());
}

TEST(Constraint, StableIdConstructionChain)
{
    Variable a{Id{1}, "a"};
    Variable b{Id{2}, "b"};
    Constraint c = ((Expression{a} + Expression{b}) == 0.0).withId(Id{100}) | Strength::strong();
    EXPECT_EQ(c.id(), Id{100});
    EXPECT_EQ(c.strength(), Strength::strong());
    EXPECT_DOUBLE_EQ(coefficientOf(c.expression(), a), 1.0);
    EXPECT_DOUBLE_EQ(coefficientOf(c.expression(), b), 1.0);
}

TEST(Constraint, ComposedExpressionWorksInConstraint)
{
    Variable left{Id{1}, "left"};
    Variable right{Id{2}, "right"};
    Variable pc{Id{3}, "pc"};
    Expression center = (Expression{left} + Expression{right}) / 2.0;
    Constraint c = center == Expression{pc};
    const Expression& e = c.expression();
    EXPECT_DOUBLE_EQ(coefficientOf(e, left), 0.5);
    EXPECT_DOUBLE_EQ(coefficientOf(e, right), 0.5);
    EXPECT_DOUBLE_EQ(coefficientOf(e, pc), -1.0);
}

TEST(Constraint, CopyIsCheapAndShares)
{
    Variable a{Id{1}, "a"};
    Constraint c1 = (Expression{a} == 5.0).withId(Id{7});
    Constraint c2 = c1;  // should share impl
    EXPECT_EQ(c1.id(), c2.id());
    EXPECT_EQ(c1.strength(), c2.strength());
}

}  // namespace arrange
