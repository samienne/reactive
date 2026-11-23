#pragma once

#include <arrange/export.h>

#include <arrange/variable.h>

#include <cstddef>
#include <memory>

namespace arrange
{

class ExpressionImpl;

class ARRANGE_API Expression
{
public:
    Expression();
    Expression(double constant);
    Expression(Variable v);
    Expression(double coefficient, Variable v);

    double constant() const noexcept;

    struct Term
    {
        double coefficient;
        Variable variable;
    };

    std::size_t termCount() const noexcept;
    Term term(std::size_t index) const;

private:
    explicit Expression(std::shared_ptr<const ExpressionImpl> impl) noexcept;

    friend ARRANGE_API Expression operator+(Expression lhs, Expression rhs);
    friend ARRANGE_API Expression operator-(Expression lhs, Expression rhs);
    friend ARRANGE_API Expression operator-(Expression expr);
    friend ARRANGE_API Expression operator*(Expression expr, double scalar);
    friend ARRANGE_API Expression operator*(double scalar, Expression expr);
    friend ARRANGE_API Expression operator/(Expression expr, double divisor);

    std::shared_ptr<const ExpressionImpl> impl_{};
};

ARRANGE_API Expression operator+(Expression lhs, Expression rhs);
ARRANGE_API Expression operator-(Expression lhs, Expression rhs);
ARRANGE_API Expression operator-(Expression expr);
ARRANGE_API Expression operator*(Expression expr, double scalar);
ARRANGE_API Expression operator*(double scalar, Expression expr);
ARRANGE_API Expression operator/(Expression expr, double divisor);

}  // namespace arrange
