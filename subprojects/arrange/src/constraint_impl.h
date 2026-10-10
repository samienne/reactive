#pragma once

#include <arrange/constraint.h>
#include <arrange/expression.h>
#include <arrange/id.h>
#include <arrange/strength.h>

namespace arrange
{

class ConstraintImpl
{
public:
    Expression expression{};
    Relation relation = Relation::eq;
    Strength strength = Strength::required();
    Id id = nullId;
};

}  // namespace arrange
