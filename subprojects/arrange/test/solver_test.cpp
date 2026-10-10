#include <arrange/constraint.h>
#include <arrange/errors.h>
#include <arrange/expression.h>
#include <arrange/solver.h>
#include <arrange/strength.h>
#include <arrange/variable.h>

#include <gtest/gtest.h>

#include <utility>
#include <vector>

namespace arrange
{

// ---- Basic equalities ------------------------------------------------------

TEST(Solver, SingleRequiredEquality)
{
    Variable x{"x"};
    Solver s;
    s.addConstraint(Expression{x} == 5.0);
    EXPECT_DOUBLE_EQ(s.valueOf(x), 5.0);
}

TEST(Solver, ChainedEqualities)
{
    Variable x{"x"};
    Variable y{"y"};
    Solver s;
    s.addConstraint(Expression{x} == 5.0);
    s.addConstraint(Expression{y} == Expression{x} + 3.0);
    EXPECT_DOUBLE_EQ(s.valueOf(x), 5.0);
    EXPECT_DOUBLE_EQ(s.valueOf(y), 8.0);
}

TEST(Solver, EqualityOfSum)
{
    Variable a{"a"};
    Variable b{"b"};
    Solver s;
    s.addConstraint(Expression{a} + Expression{b} == 10.0);
    s.addConstraint(Expression{a} == 3.0);
    EXPECT_DOUBLE_EQ(s.valueOf(a), 3.0);
    EXPECT_DOUBLE_EQ(s.valueOf(b), 7.0);
}

TEST(Solver, UnseenVariableIsZero)
{
    Variable x{"x"};
    Solver s;
    EXPECT_DOUBLE_EQ(s.valueOf(x), 0.0);
    EXPECT_FALSE(s.contains(x));
}

TEST(Solver, ContainsTracksUsedVariables)
{
    Variable x{"x"};
    Variable y{"y"};
    Solver s;
    s.addConstraint(Expression{x} == 1.0);
    EXPECT_TRUE(s.contains(x));
    EXPECT_FALSE(s.contains(y));
}

// ---- Inequalities ---------------------------------------------------------

TEST(Solver, InequalityWithStay)
{
    Variable x{"x"};
    Solver s;
    // x >= 5 alone — solver can pick any x >= 5; with no other pressure
    // it'll pick the binding value 5.
    s.addConstraint(Expression{x} >= 5.0);
    EXPECT_GE(s.valueOf(x), 5.0 - 1e-8);
}

TEST(Solver, InequalityWithRequiredMinimum)
{
    Variable x{"x"};
    Solver s;
    s.addConstraint(Expression{x} >= 10.0);
    s.addConstraint((Expression{x} == 5.0) | Strength::weak());
    // weak "== 5" loses to required ">= 10"; x should be 10 (tight).
    EXPECT_DOUBLE_EQ(s.valueOf(x), 10.0);
}

TEST(Solver, InequalityLessOrEqual)
{
    Variable x{"x"};
    Solver s;
    s.addConstraint(Expression{x} <= 100.0);
    s.addConstraint((Expression{x} == 200.0) | Strength::weak());
    EXPECT_DOUBLE_EQ(s.valueOf(x), 100.0);
}

// ---- Strength arbitration -------------------------------------------------

TEST(Solver, StrongBeatsWeak)
{
    Variable x{"x"};
    Solver s;
    s.addConstraint((Expression{x} == 10.0) | Strength::weak());
    s.addConstraint((Expression{x} == 20.0) | Strength::strong());
    EXPECT_DOUBLE_EQ(s.valueOf(x), 20.0);
}

TEST(Solver, RequiredBeatsStrong)
{
    Variable x{"x"};
    Solver s;
    s.addConstraint((Expression{x} == 10.0) | Strength::strong());
    s.addConstraint(Expression{x} == 20.0);
    EXPECT_DOUBLE_EQ(s.valueOf(x), 20.0);
}

// ---- Unsatisfiable --------------------------------------------------------

TEST(Solver, UnsatisfiableRequiredPairThrows)
{
    Variable x{"x"};
    Solver s;
    s.addConstraint(Expression{x} == 5.0);
    EXPECT_THROW(s.addConstraint(Expression{x} == 10.0), UnsatisfiableConstraint);
}

TEST(Solver, UnsatisfiableRequiredInequalityThrows)
{
    Variable x{"x"};
    Solver s;
    s.addConstraint(Expression{x} >= 10.0);
    EXPECT_THROW(s.addConstraint(Expression{x} <= 5.0), UnsatisfiableConstraint);
}

// ---- Duplicate ids --------------------------------------------------------

TEST(Solver, DuplicateConstraintIdThrows)
{
    Variable x{"x"};
    Solver s;
    s.addConstraint((Expression{x} == 5.0).withId(Id{42}));
    EXPECT_THROW(s.addConstraint((Expression{x} == 5.0).withId(Id{42})), DuplicateConstraint);
}

TEST(Solver, DuplicateNullIdAllowed)
{
    Variable x{"x"};
    Solver s;
    s.addConstraint(Expression{x} >= 0.0);
    // Second nullId constraint must be accepted (distinct object, different tableau effect).
    s.addConstraint(Expression{x} <= 100.0);
    // x with no pressure should still satisfy both.
    EXPECT_GE(s.valueOf(x), 0.0 - 1e-8);
    EXPECT_LE(s.valueOf(x), 100.0 + 1e-8);
}

// ---- Composed expression example from the spec ----------------------------

// ---- Remove constraint ----------------------------------------------------

TEST(Solver, RemoveByIdRestoresFreedom)
{
    Variable x{"x"};
    Solver s;
    s.addConstraint((Expression{x} == 5.0).withId(Id{1}));
    s.addConstraint((Expression{x} >= 0.0).withId(Id{2}));
    EXPECT_DOUBLE_EQ(s.valueOf(x), 5.0);

    s.removeConstraint((Expression{x} == 5.0).withId(Id{1}));
    // Now only x >= 0. After removal x is no longer pinned to 5.
    // We don't assert the specific value (solver-dependent) but we can
    // assert the remaining constraint still holds.
    EXPECT_GE(s.valueOf(x), 0.0 - 1e-8);
}

TEST(Solver, RemoveNullIdConstraintThrows)
{
    Variable x{"x"};
    Solver s;
    // Can't remove by id when id is nullId and the object differs.
    Constraint c = Expression{x} == 5.0;  // nullId
    s.addConstraint(c);
    // Passing a different nullId Constraint (new impl) must not be removable via id.
    Constraint other = Expression{x} == 5.0;
    EXPECT_THROW(s.removeConstraint(other), UnknownConstraint);
    // But the original object can be removed (same impl).
    EXPECT_NO_THROW(s.removeConstraint(c));
}

TEST(Solver, RemoveNonExistentIdThrows)
{
    Solver s;
    Variable x{"x"};
    EXPECT_THROW(s.removeConstraint((Expression{x} == 5.0).withId(Id{99})), UnknownConstraint);
}

TEST(Solver, HasConstraintIsIdentityBased)
{
    Variable x{"x"};
    Solver s;
    Constraint c = (Expression{x} == 5.0).withId(Id{1});
    s.addConstraint(c);
    EXPECT_TRUE(s.hasConstraint(c));
    Constraint copy = c;  // shares impl
    EXPECT_TRUE(s.hasConstraint(copy));
    // Different Constraint with same id but different impl — not "has" by impl.
    Constraint lookalike = (Expression{x} == 5.0).withId(Id{1});
    EXPECT_FALSE(s.hasConstraint(lookalike));
}

// ---- Edit variables -------------------------------------------------------

TEST(Solver, EditVariableSuggestValue)
{
    Variable x{"x"};
    Solver s;
    s.addEditVariable(x, Strength::strong());
    s.suggestValue(x, 42.0);
    EXPECT_DOUBLE_EQ(s.valueOf(x), 42.0);
}

TEST(Solver, EditVariableDragScenario)
{
    // Classic scenario: x in [0, 100], a drag handle at x.
    Variable x{"x"};
    Solver s;
    s.addConstraint(Expression{x} >= 0.0);
    s.addConstraint(Expression{x} <= 100.0);
    s.addEditVariable(x, Strength::strong());

    s.suggestValue(x, 50.0);
    EXPECT_DOUBLE_EQ(s.valueOf(x), 50.0);
    s.suggestValue(x, 10.0);
    EXPECT_DOUBLE_EQ(s.valueOf(x), 10.0);
    s.suggestValue(x, 200.0);  // clipped by required x <= 100
    EXPECT_DOUBLE_EQ(s.valueOf(x), 100.0);
    s.suggestValue(x, -10.0);  // clipped by required x >= 0
    EXPECT_DOUBLE_EQ(s.valueOf(x), 0.0);
}

TEST(Solver, EditVariableRequiredStrengthRejected)
{
    Variable x{"x"};
    Solver s;
    EXPECT_THROW(s.addEditVariable(x, Strength::required()), BadStrength);
}

TEST(Solver, DuplicateEditVariableRejected)
{
    Variable x{"x"};
    Solver s;
    s.addEditVariable(x, Strength::strong());
    EXPECT_THROW(s.addEditVariable(x, Strength::strong()), DuplicateEditVariable);
}

TEST(Solver, UnknownEditVariableSuggestThrows)
{
    Variable x{"x"};
    Solver s;
    EXPECT_THROW(s.suggestValue(x, 5.0), UnknownEditVariable);
}

TEST(Solver, RemoveEditVariable)
{
    Variable x{"x"};
    Solver s;
    s.addEditVariable(x, Strength::strong());
    EXPECT_TRUE(s.hasEditVariable(x));
    s.removeEditVariable(x);
    EXPECT_FALSE(s.hasEditVariable(x));
    EXPECT_THROW(s.suggestValue(x, 5.0), UnknownEditVariable);
}

TEST(Solver, RemoveUnknownEditVariableThrows)
{
    Variable x{"x"};
    Solver s;
    EXPECT_THROW(s.removeEditVariable(x), UnknownEditVariable);
}

// ---- setConstraints -------------------------------------------------------

TEST(Solver, SetConstraintsFromEmpty)
{
    Variable x{"x"};
    Solver s;
    std::vector<Constraint> desired{
        (Expression{x} == 5.0).withId(Id{1}),
    };
    auto diff = s.setConstraints(desired);
    EXPECT_EQ(diff.added.size(), 1u);
    EXPECT_EQ(diff.removed.size(), 0u);
    EXPECT_DOUBLE_EQ(s.valueOf(x), 5.0);
}

TEST(Solver, SetConstraintsRemovesVanished)
{
    Variable x{"x"};
    Solver s;
    Constraint c1 = (Expression{x} == 5.0).withId(Id{1});
    Constraint c2 = (Expression{x} >= 0.0).withId(Id{2});
    s.addConstraint(c1);
    s.addConstraint(c2);
    EXPECT_DOUBLE_EQ(s.valueOf(x), 5.0);

    // Desired: only c2 remains; c1 is removed; new c3 is added.
    Constraint c3 = (Expression{x} == 7.0).withId(Id{3});
    auto diff = s.setConstraints({c2, c3});
    EXPECT_EQ(diff.added.size(), 1u);
    EXPECT_EQ(diff.added[0].id(), Id{3});
    EXPECT_EQ(diff.removed.size(), 1u);
    EXPECT_EQ(diff.removed[0].id(), Id{1});
    EXPECT_DOUBLE_EQ(s.valueOf(x), 7.0);
}

TEST(Solver, SetConstraintsNoOpForMatchingIds)
{
    Variable x{"x"};
    Solver s;
    Constraint c = (Expression{x} == 5.0).withId(Id{1});
    s.addConstraint(c);

    auto diff = s.setConstraints({c});
    EXPECT_TRUE(diff.added.empty());
    EXPECT_TRUE(diff.removed.empty());
    EXPECT_DOUBLE_EQ(s.valueOf(x), 5.0);
}

TEST(Solver, SetConstraintsRollsBackOnFailure)
{
    Variable x{"x"};
    Solver s;
    Constraint c1 = (Expression{x} == 5.0).withId(Id{1});
    s.addConstraint(c1);

    // This set drops c1 (OK) but adds two contradictory required constraints.
    Constraint c2 = (Expression{x} == 10.0).withId(Id{2});
    Constraint c3 = (Expression{x} == 20.0).withId(Id{3});
    EXPECT_THROW(s.setConstraints({c2, c3}), UnsatisfiableConstraint);

    // State must be restored: c1 is still there, x is still 5.
    EXPECT_DOUBLE_EQ(s.valueOf(x), 5.0);
    EXPECT_TRUE(s.hasConstraint(c1));
}

// ---- Reset ----------------------------------------------------------------

TEST(Solver, ResetClearsEverything)
{
    Variable x{"x"};
    Solver s;
    s.addConstraint(Expression{x} == 5.0);
    EXPECT_DOUBLE_EQ(s.valueOf(x), 5.0);
    s.reset();
    EXPECT_FALSE(s.contains(x));
    EXPECT_DOUBLE_EQ(s.valueOf(x), 0.0);
}

TEST(Solver, ResetAllowsReusingIds)
{
    Variable x{"x"};
    Solver s;
    Constraint c = (Expression{x} == 5.0).withId(Id{1});
    s.addConstraint(c);
    s.reset();
    // Id{1} is free again.
    EXPECT_NO_THROW(s.addConstraint(c));
    EXPECT_DOUBLE_EQ(s.valueOf(x), 5.0);
}

// ---- Copy / move ----------------------------------------------------------

TEST(Solver, CopyIsIndependent)
{
    Variable x{"x"};
    Solver a;
    a.addConstraint(Expression{x} == 5.0);

    Solver b = a;  // deep copy
    EXPECT_DOUBLE_EQ(b.valueOf(x), 5.0);

    // Mutate b, verify a unchanged.
    b.addConstraint((Expression{x} == 5.0) | Strength::weak());  // redundant weak, no change
    b.reset();
    b.addConstraint(Expression{x} == 99.0);

    EXPECT_DOUBLE_EQ(a.valueOf(x), 5.0);
    EXPECT_DOUBLE_EQ(b.valueOf(x), 99.0);
}

TEST(Solver, MoveTransfersState)
{
    Variable x{"x"};
    Solver a;
    a.addConstraint(Expression{x} == 5.0);
    Solver b = std::move(a);
    EXPECT_DOUBLE_EQ(b.valueOf(x), 5.0);
}

TEST(Solver, CenterConstraint)
{
    Variable left{"left"};
    Variable right{"right"};
    Variable pc{"pc"};
    Solver s;
    s.addConstraint(Expression{left} >= 0.0);
    s.addConstraint(Expression{right} == Expression{left} + 100.0);
    s.addConstraint(Expression{pc} == 300.0);
    Expression center = (Expression{left} + Expression{right}) / 2.0;
    s.addConstraint(center == Expression{pc});

    EXPECT_NEAR(s.valueOf(left), 250.0, 1e-8);
    EXPECT_NEAR(s.valueOf(right), 350.0, 1e-8);
    EXPECT_NEAR(s.valueOf(pc), 300.0, 1e-8);
}

}  // namespace arrange
