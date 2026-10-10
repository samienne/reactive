#pragma once

#include <arrange/variable.h>

#include <cstddef>
#include <memory>

namespace arrange
{

class ExpressionImpl;

class Expression
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

    friend Expression operator+(Expression lhs, Expression rhs);
    friend Expression operator-(Expression lhs, Expression rhs);
    friend Expression operator-(Expression expr);
    friend Expression operator*(Expression expr, double scalar);
    friend Expression operator*(double scalar, Expression expr);
    friend Expression operator/(Expression expr, double divisor);

    std::shared_ptr<const ExpressionImpl> impl_{};
};

Expression operator+(Expression lhs, Expression rhs);
Expression operator-(Expression lhs, Expression rhs);
Expression operator-(Expression expr);
Expression operator*(Expression expr, double scalar);
Expression operator*(double scalar, Expression expr);
Expression operator/(Expression expr, double divisor);

}  // namespace arrange
