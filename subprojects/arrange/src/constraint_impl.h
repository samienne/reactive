#pragma once

#include <arrange/expression.h>
#include <arrange/id.h>
#include <arrange/strength.h>

namespace arrange
{

enum class Relation
{
    eq,
    le,
    ge
};

class ConstraintImpl
{
public:
    Expression expression{};
    Relation relation = Relation::eq;
    Strength strength = Strength::required();
    Id id = nullId;
};

}  // namespace arrange
