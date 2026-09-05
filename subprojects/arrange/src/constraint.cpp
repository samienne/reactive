#include "constraint_impl.h"

#include <arrange/constraint.h>

#include <memory>
#include <utility>

namespace arrange
{

namespace
{

std::shared_ptr<const ConstraintImpl> makeImpl(
    Expression expr, Relation relation, Strength strength, Id id)
{
    auto impl = std::make_shared<ConstraintImpl>();
    impl->expression = std::move(expr);
    impl->relation = relation;
    impl->strength = strength;
    impl->id = id;
    return impl;
}

}  // namespace

Constraint::Constraint(std::shared_ptr<const ConstraintImpl> impl) noexcept
    : impl_(std::move(impl))
{
}

Id Constraint::id() const noexcept
{
    return impl_ ? impl_->id : nullId;
}

const Expression& Constraint::expression() const noexcept
{
    return impl_->expression;
}

Strength Constraint::strength() const noexcept
{
    return impl_ ? impl_->strength : Strength::required();
}

Constraint Constraint::withId(Id id) const
{
    return Constraint{makeImpl(impl_->expression, impl_->relation, impl_->strength, id)};
}

Constraint Constraint::withStrength(Strength strength) const
{
    return Constraint{makeImpl(impl_->expression, impl_->relation, strength, impl_->id)};
}

Constraint operator==(Expression lhs, Expression rhs)
{
    return Constraint{makeImpl(lhs - rhs, Relation::eq, Strength::required(), nullId)};
}

Constraint operator<=(Expression lhs, Expression rhs)
{
    return Constraint{makeImpl(lhs - rhs, Relation::le, Strength::required(), nullId)};
}

Constraint operator>=(Expression lhs, Expression rhs)
{
    return Constraint{makeImpl(lhs - rhs, Relation::ge, Strength::required(), nullId)};
}

Constraint operator|(Constraint c, Strength s)
{
    return c.withStrength(s);
}

Constraint operator|(Strength s, Constraint c)
{
    return c.withStrength(s);
}

}  // namespace arrange
