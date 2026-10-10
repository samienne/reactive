#pragma once

#include <arrange/expression.h>
#include <arrange/id.h>
#include <arrange/strength.h>

#include <memory>

namespace arrange
{

class ConstraintImpl;
class SolverImpl;

class Constraint
{
public:
    Constraint(const Constraint&) = default;
    Constraint(Constraint&&) noexcept = default;
    Constraint& operator=(const Constraint&) = default;
    Constraint& operator=(Constraint&&) noexcept = default;

    Id id() const noexcept;
    const Expression& expression() const noexcept;
    Strength strength() const noexcept;

    Constraint withId(Id id) const;
    Constraint withStrength(Strength strength) const;

private:
    friend Constraint operator==(Expression lhs, Expression rhs);
    friend Constraint operator<=(Expression lhs, Expression rhs);
    friend Constraint operator>=(Expression lhs, Expression rhs);
    friend class SolverImpl;

    explicit Constraint(std::shared_ptr<const ConstraintImpl> impl) noexcept;

    std::shared_ptr<const ConstraintImpl> impl_{};
};

Constraint operator==(Expression lhs, Expression rhs);
Constraint operator<=(Expression lhs, Expression rhs);
Constraint operator>=(Expression lhs, Expression rhs);

Constraint operator|(Constraint c, Strength s);
Constraint operator|(Strength s, Constraint c);

}  // namespace arrange
