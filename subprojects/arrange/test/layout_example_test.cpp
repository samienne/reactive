// Integration tests — classic UI layout scenarios that exercise the solver
// end-to-end through the public API.

#include <arrange/arrange.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace arrange
{

// ---- Spec §8.1 — one-shot solve with an edit drag handle ------------------

TEST(Layout, SpecOneShotWithDrag)
{
    Variable left{"left"};
    Variable right{"right"};
    Variable width{"width"};

    Solver s;
    s.addConstraint(Expression{left} >= 0.0);
    s.addConstraint(Expression{right} == Expression{left} + Expression{width});
    s.addConstraint(Expression{width} >= 100.0);
    s.addConstraint((Expression{right} == 500.0) | Strength::strong());

    s.addEditVariable(left, Strength::medium());
    s.suggestValue(left, 50.0);

    EXPECT_DOUBLE_EQ(s.valueOf(left), 50.0);
    // Strong "right == 500" pulls right to 500, so width = 450 (>= 100 satisfied).
    EXPECT_DOUBLE_EQ(s.valueOf(right), 500.0);
    EXPECT_DOUBLE_EQ(s.valueOf(width), 450.0);
}

// ---- Three-column layout --------------------------------------------------
//
// Classic Cassowary demo: three columns of equal width, fitting in a
// container of known width with a fixed gap between columns.
//
// Given container width W and gap g:
//   col1.left == 0
//   col3.right == W
//   col2.left == col1.right + g
//   col3.left == col2.right + g
//   col1.width == col2.width == col3.width  (equal widths)
//   coli.right == coli.left + coli.width

TEST(Layout, ThreeEqualColumnsInContainer)
{
    Variable l1{"l1"}, r1{"r1"}, w1{"w1"};
    Variable l2{"l2"}, r2{"r2"}, w2{"w2"};
    Variable l3{"l3"}, r3{"r3"}, w3{"w3"};

    const double W = 320.0;
    const double g = 10.0;

    Solver s;
    s.addConstraint(Expression{l1} == 0.0);
    s.addConstraint(Expression{r3} == W);
    s.addConstraint(Expression{r1} == Expression{l1} + Expression{w1});
    s.addConstraint(Expression{r2} == Expression{l2} + Expression{w2});
    s.addConstraint(Expression{r3} == Expression{l3} + Expression{w3});
    s.addConstraint(Expression{l2} == Expression{r1} + g);
    s.addConstraint(Expression{l3} == Expression{r2} + g);
    s.addConstraint(Expression{w1} == Expression{w2});
    s.addConstraint(Expression{w2} == Expression{w3});

    // W - 2g = 3w  →  w = (320 - 20) / 3 = 100
    EXPECT_DOUBLE_EQ(s.valueOf(w1), 100.0);
    EXPECT_DOUBLE_EQ(s.valueOf(w2), 100.0);
    EXPECT_DOUBLE_EQ(s.valueOf(w3), 100.0);
    EXPECT_DOUBLE_EQ(s.valueOf(l1), 0.0);
    EXPECT_DOUBLE_EQ(s.valueOf(r1), 100.0);
    EXPECT_DOUBLE_EQ(s.valueOf(l2), 110.0);
    EXPECT_DOUBLE_EQ(s.valueOf(r2), 210.0);
    EXPECT_DOUBLE_EQ(s.valueOf(l3), 220.0);
    EXPECT_DOUBLE_EQ(s.valueOf(r3), 320.0);
}

// ---- Window resize --------------------------------------------------------
//
// Make the container width a variable that the user can suggest; widths
// must reflow.

TEST(Layout, ResizableThreeColumnLayout)
{
    Variable W{"W"};
    Variable l1{"l1"}, r1{"r1"}, w1{"w1"};
    Variable l2{"l2"}, r2{"r2"}, w2{"w2"};
    Variable l3{"l3"}, r3{"r3"}, w3{"w3"};
    const double g = 10.0;

    Solver s;
    s.addConstraint(Expression{l1} == 0.0);
    s.addConstraint(Expression{r3} == Expression{W});
    s.addConstraint(Expression{r1} == Expression{l1} + Expression{w1});
    s.addConstraint(Expression{r2} == Expression{l2} + Expression{w2});
    s.addConstraint(Expression{r3} == Expression{l3} + Expression{w3});
    s.addConstraint(Expression{l2} == Expression{r1} + g);
    s.addConstraint(Expression{l3} == Expression{r2} + g);
    s.addConstraint(Expression{w1} == Expression{w2});
    s.addConstraint(Expression{w2} == Expression{w3});

    s.addEditVariable(W, Strength::strong());

    s.suggestValue(W, 320.0);
    EXPECT_DOUBLE_EQ(s.valueOf(w1), 100.0);
    EXPECT_DOUBLE_EQ(s.valueOf(r3), 320.0);

    s.suggestValue(W, 620.0);  // new size
    EXPECT_DOUBLE_EQ(s.valueOf(w1), 200.0);
    EXPECT_DOUBLE_EQ(s.valueOf(r3), 620.0);

    s.suggestValue(W, 80.0);  // tiny — widths become (80 - 20) / 3 = 20
    EXPECT_DOUBLE_EQ(s.valueOf(w1), 20.0);
    EXPECT_DOUBLE_EQ(s.valueOf(r3), 80.0);
}

// ---- Stay at current value using a weak equality --------------------------

TEST(Layout, WeakStayHoldsPositionUnderFreedom)
{
    Variable x{"x"};
    Solver s;

    // x can freely move, but prefers to stay at 42.
    s.addConstraint((Expression{x} == 42.0) | Strength::weak());
    s.addConstraint(Expression{x} >= 0.0);
    s.addConstraint(Expression{x} <= 100.0);

    EXPECT_DOUBLE_EQ(s.valueOf(x), 42.0);
}

TEST(Layout, StrongStayWinsAgainstRangeAndWeak)
{
    Variable x{"x"};
    Solver s;
    s.addConstraint((Expression{x} == 10.0) | Strength::weak());
    s.addConstraint((Expression{x} == 80.0) | Strength::strong());
    s.addConstraint(Expression{x} >= 0.0);
    s.addConstraint(Expression{x} <= 100.0);

    EXPECT_DOUBLE_EQ(s.valueOf(x), 80.0);
}

// ---- Diff-based incremental update (spec §8.2 workflow) -------------------
//
// Simulate a UI toolkit rebuilding its constraint list each layout pass and
// handing it to the solver; verify matching ids cause no churn while new
// widgets get added and removed ones are dropped.

TEST(Layout, DiffDrivenRebuild)
{
    Variable left{Id{1}, "left"};
    Variable right{Id{2}, "right"};

    // Pass 1: a single widget pinned at 10..30.
    std::vector<Constraint> pass1{
        (Expression{left} == 10.0).withId(Id{101}),
        (Expression{right} == 30.0).withId(Id{102}),
    };

    Solver s;
    auto d1 = s.setConstraints(pass1);
    EXPECT_EQ(d1.added.size(), 2u);
    EXPECT_EQ(d1.removed.size(), 0u);
    EXPECT_DOUBLE_EQ(s.valueOf(left), 10.0);
    EXPECT_DOUBLE_EQ(s.valueOf(right), 30.0);

    // Pass 2: right moved to 50; left unchanged (same id+same shape).
    std::vector<Constraint> pass2{
        (Expression{left} == 10.0).withId(Id{101}),  // same id, same content
        (Expression{right} == 50.0).withId(Id{103}),  // new id; old one dropped
    };
    auto d2 = s.setConstraints(pass2);
    EXPECT_EQ(d2.added.size(), 1u);
    EXPECT_EQ(d2.added[0].id(), Id{103});
    EXPECT_EQ(d2.removed.size(), 1u);
    EXPECT_EQ(d2.removed[0].id(), Id{102});
    EXPECT_DOUBLE_EQ(s.valueOf(left), 10.0);
    EXPECT_DOUBLE_EQ(s.valueOf(right), 50.0);

    // Pass 3: identical to pass 2 → zero work.
    auto d3 = s.setConstraints(pass2);
    EXPECT_TRUE(d3.added.empty());
    EXPECT_TRUE(d3.removed.empty());
}

// ---- Edit + remove interplay ----------------------------------------------

TEST(Layout, RemoveConstraintWhileEditActive)
{
    Variable x{"x"};
    Solver s;
    Constraint bound = (Expression{x} <= 100.0).withId(Id{1});
    s.addConstraint(bound);
    s.addEditVariable(x, Strength::strong());

    s.suggestValue(x, 80.0);
    EXPECT_DOUBLE_EQ(s.valueOf(x), 80.0);

    s.suggestValue(x, 150.0);
    EXPECT_DOUBLE_EQ(s.valueOf(x), 100.0);  // clipped by bound

    s.removeConstraint(bound);
    s.suggestValue(x, 150.0);
    EXPECT_DOUBLE_EQ(s.valueOf(x), 150.0);  // unclipped now
}

// ---- Large-ish constraint set sanity --------------------------------------
//
// 50 variables in a chain: v_{i+1} == v_i + 1. Then pin v_0. Everything else
// should follow.

TEST(Layout, FiftyVariableChain)
{
    constexpr int N = 50;
    std::vector<Variable> v;
    v.reserve(N);
    for (int i = 0; i < N; ++i)
    {
        v.emplace_back(Id{static_cast<std::uint64_t>(i + 1)}, "v" + std::to_string(i));
    }

    Solver s;
    s.addConstraint(Expression{v[0]} == 0.0);
    for (int i = 1; i < N; ++i)
    {
        s.addConstraint(Expression{v[i]} == Expression{v[i - 1]} + 1.0);
    }

    for (int i = 0; i < N; ++i)
    {
        EXPECT_DOUBLE_EQ(s.valueOf(v[i]), static_cast<double>(i));
    }
}

}  // namespace arrange
