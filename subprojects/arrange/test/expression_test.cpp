#include <arrange/expression.h>
#include <arrange/variable.h>

#include <gtest/gtest.h>

#include <stdexcept>

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

TEST(Expression, DefaultIsZero)
{
    Expression e;
    EXPECT_DOUBLE_EQ(e.constant(), 0.0);
    EXPECT_EQ(e.termCount(), 0u);
}

TEST(Expression, FromConstant)
{
    Expression e{3.5};
    EXPECT_DOUBLE_EQ(e.constant(), 3.5);
    EXPECT_EQ(e.termCount(), 0u);
}

TEST(Expression, FromVariableImpliesUnitCoefficient)
{
    Variable v{Id{1}, "v"};
    Expression e{v};
    EXPECT_DOUBLE_EQ(e.constant(), 0.0);
    ASSERT_EQ(e.termCount(), 1u);
    EXPECT_DOUBLE_EQ(e.term(0).coefficient, 1.0);
    EXPECT_EQ(e.term(0).variable, v);
}

TEST(Expression, FromCoefficientAndVariable)
{
    Variable v{Id{1}, "v"};
    Expression e{2.5, v};
    EXPECT_DOUBLE_EQ(e.constant(), 0.0);
    ASSERT_EQ(e.termCount(), 1u);
    EXPECT_DOUBLE_EQ(e.term(0).coefficient, 2.5);
}

TEST(Expression, TermIndexOutOfRangeThrows)
{
    Expression e;
    EXPECT_THROW(e.term(0), std::out_of_range);
}

TEST(Expression, AdditionSumsConstantsAndMergesTerms)
{
    Variable a{Id{1}, "a"};
    Variable b{Id{2}, "b"};
    Expression e = Expression{1.0, a} + Expression{2.0, a} + Expression{3.0, b} + 4.0;

    EXPECT_DOUBLE_EQ(e.constant(), 4.0);
    EXPECT_DOUBLE_EQ(coefficientOf(e, a), 3.0);
    EXPECT_DOUBLE_EQ(coefficientOf(e, b), 3.0);
    EXPECT_EQ(e.termCount(), 2u);
}

TEST(Expression, SubtractionNegatesRhsTerms)
{
    Variable a{Id{1}, "a"};
    Expression e = Expression{5.0, a} - Expression{2.0, a};
    EXPECT_EQ(e.termCount(), 1u);
    EXPECT_DOUBLE_EQ(coefficientOf(e, a), 3.0);
}

TEST(Expression, SelfSubtractionCancels)
{
    Variable a{Id{1}, "a"};
    Expression e = Expression{a} - Expression{a};
    EXPECT_EQ(e.termCount(), 0u);
    EXPECT_DOUBLE_EQ(e.constant(), 0.0);
}

TEST(Expression, UnaryNegationFlipsAll)
{
    Variable a{Id{1}, "a"};
    Expression e = -(Expression{2.0, a} + 3.0);
    EXPECT_DOUBLE_EQ(e.constant(), -3.0);
    EXPECT_DOUBLE_EQ(coefficientOf(e, a), -2.0);
}

TEST(Expression, ScalarMultiplyBothSides)
{
    Variable a{Id{1}, "a"};
    Expression lhs = Expression{2.0, a} * 3.0;
    Expression rhs = 3.0 * Expression{2.0, a};
    EXPECT_DOUBLE_EQ(coefficientOf(lhs, a), 6.0);
    EXPECT_DOUBLE_EQ(coefficientOf(rhs, a), 6.0);
}

TEST(Expression, ScalarMultiplyByZeroYieldsEmpty)
{
    Variable a{Id{1}, "a"};
    Expression e = Expression{5.0, a} * 0.0;
    EXPECT_EQ(e.termCount(), 0u);
    EXPECT_DOUBLE_EQ(e.constant(), 0.0);
}

TEST(Expression, DivisionByScalar)
{
    Variable a{Id{1}, "a"};
    Expression e = Expression{10.0, a} / 4.0;
    EXPECT_DOUBLE_EQ(coefficientOf(e, a), 2.5);
}

TEST(Expression, DivisionByZeroThrows)
{
    Variable a{Id{1}, "a"};
    Expression e{1.0, a};
    EXPECT_THROW(e / 0.0, std::invalid_argument);
}

TEST(Expression, ComposedCenter)
{
    // A realistic example from the spec: center = (left + right) / 2.
    Variable left{Id{1}, "left"};
    Variable right{Id{2}, "right"};
    Expression center = (Expression{left} + Expression{right}) / 2.0;
    EXPECT_DOUBLE_EQ(coefficientOf(center, left), 0.5);
    EXPECT_DOUBLE_EQ(coefficientOf(center, right), 0.5);
    EXPECT_DOUBLE_EQ(center.constant(), 0.0);
}

TEST(Expression, VariableImplicitlyConvertsForArithmetic)
{
    Variable a{Id{1}, "a"};
    Variable b{Id{2}, "b"};
    // Requires implicit conversion from Variable to Expression.
    Expression e = Expression{a} + b - 3.0;
    EXPECT_DOUBLE_EQ(coefficientOf(e, a), 1.0);
    EXPECT_DOUBLE_EQ(coefficientOf(e, b), 1.0);
    EXPECT_DOUBLE_EQ(e.constant(), -3.0);
}

TEST(Expression, TermsOrderedByVariableId)
{
    Variable v3{Id{3}, "v3"};
    Variable v1{Id{1}, "v1"};
    Variable v2{Id{2}, "v2"};
    Expression e = Expression{v3} + Expression{v1} + Expression{v2};
    ASSERT_EQ(e.termCount(), 3u);
    EXPECT_EQ(e.term(0).variable, v1);
    EXPECT_EQ(e.term(1).variable, v2);
    EXPECT_EQ(e.term(2).variable, v3);
}

}  // namespace arrange
