#pragma once

#include <arrange/expression.h>

#include <vector>

namespace arrange
{

class ExpressionImpl
{
public:
    double constant;
    std::vector<Expression::Term> terms;
};

}  // namespace arrange
