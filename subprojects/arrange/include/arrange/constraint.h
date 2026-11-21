#pragma once

#include <arrange/export.h>

#include <arrange/expression.h>
#include <arrange/id.h>
#include <arrange/strength.h>

#include <memory>

namespace arrange
{

class ConstraintImpl;
class SolverImpl;

enum class Relation
{
    eq,
    le,
    ge
};

class ARRANGE_API Constraint
{
public:
    Constraint(const Constraint&) = default;
    Constraint(Constraint&&) noexcept = default;
    Constraint& operator=(const Constraint&) = default;
    Constraint& operator=(Constraint&&) noexcept = default;

    Id id() const noexcept;
    const Expression& expression() const noexcept;
    Strength strength() const noexcept;
    Relation relation() const noexcept;

    Constraint withId(Id id) const;
    Constraint withStrength(Strength strength) const;

private:
    friend ARRANGE_API Constraint operator==(Expression lhs, Expression rhs);
    friend ARRANGE_API Constraint operator<=(Expression lhs, Expression rhs);
    friend ARRANGE_API Constraint operator>=(Expression lhs, Expression rhs);
    friend class SolverImpl;

    explicit Constraint(std::shared_ptr<const ConstraintImpl> impl) noexcept;

    std::shared_ptr<const ConstraintImpl> impl_{};
};

ARRANGE_API Constraint operator==(Expression lhs, Expression rhs);
ARRANGE_API Constraint operator<=(Expression lhs, Expression rhs);
ARRANGE_API Constraint operator>=(Expression lhs, Expression rhs);

ARRANGE_API Constraint operator|(Constraint c, Strength s);
ARRANGE_API Constraint operator|(Strength s, Constraint c);

}  // namespace arrange
