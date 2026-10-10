#pragma once

#include <arrange/export.h>

#include <arrange/constraint.h>

#include <vector>

namespace arrange
{

struct ConstraintDiff
{
    std::vector<Constraint> added;
    std::vector<Constraint> removed;
};

ARRANGE_API ConstraintDiff diffConstraints(
    const std::vector<Constraint>& before, const std::vector<Constraint>& after);

}  // namespace arrange
