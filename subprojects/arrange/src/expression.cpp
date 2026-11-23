#include "expression_impl.h"

#include <arrange/expression.h>

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace arrange
{

namespace
{

std::vector<Expression::Term> canonicalizeTerms(std::vector<Expression::Term> terms)
{
    std::sort(terms.begin(),
        terms.end(),
        [](const Expression::Term& a, const Expression::Term& b)
        { return a.variable.id() < b.variable.id(); });
    std::vector<Expression::Term> out;
    out.reserve(terms.size());
    for (auto& t : terms)
    {
        if (!out.empty() && out.back().variable.id() == t.variable.id())
        {
            out.back().coefficient += t.coefficient;
        }
        else
        {
            out.push_back(std::move(t));
        }
    }
    out.erase(
        std::remove_if(
            out.begin(), out.end(), [](const Expression::Term& t) { return t.coefficient == 0.0; }),
        out.end());
    return out;
}

std::shared_ptr<const ExpressionImpl> makeImpl(double constant, std::vector<Expression::Term> terms)
{
    auto impl = std::make_shared<ExpressionImpl>();
    impl->constant = constant;
    impl->terms = canonicalizeTerms(std::move(terms));
    return impl;
}

std::vector<Expression::Term> copyTerms(const ExpressionImpl& impl)
{
    return impl.terms;
}

}  // namespace

Expression::Expression()
    : impl_(makeImpl(0.0, {}))
{
}

Expression::Expression(double constant)
    : impl_(makeImpl(constant, {}))
{
}

Expression::Expression(Variable v)
    : impl_(makeImpl(0.0, {{1.0, std::move(v)}}))
{
}

Expression::Expression(double coefficient, Variable v)
    : impl_(makeImpl(0.0, {{coefficient, std::move(v)}}))
{
}

Expression::Expression(std::shared_ptr<const ExpressionImpl> impl) noexcept
    : impl_(std::move(impl))
{
}

double Expression::constant() const noexcept
{
    return impl_->constant;
}

std::size_t Expression::termCount() const noexcept
{
    return impl_->terms.size();
}

Expression::Term Expression::term(std::size_t index) const
{
    if (index >= impl_->terms.size())
    {
        throw std::out_of_range{"Expression::term index out of range"};
    }
    return impl_->terms[index];
}

Expression operator+(Expression lhs, Expression rhs)
{
    std::vector<Expression::Term> terms = copyTerms(*lhs.impl_);
    const auto& rhsTerms = rhs.impl_->terms;
    terms.insert(terms.end(), rhsTerms.begin(), rhsTerms.end());
    return Expression{makeImpl(lhs.impl_->constant + rhs.impl_->constant, std::move(terms))};
}

Expression operator-(Expression lhs, Expression rhs)
{
    std::vector<Expression::Term> terms = copyTerms(*lhs.impl_);
    terms.reserve(terms.size() + rhs.impl_->terms.size());
    for (const auto& t : rhs.impl_->terms)
    {
        terms.push_back({-t.coefficient, t.variable});
    }
    return Expression{makeImpl(lhs.impl_->constant - rhs.impl_->constant, std::move(terms))};
}

Expression operator-(Expression expr)
{
    std::vector<Expression::Term> terms;
    terms.reserve(expr.impl_->terms.size());
    for (const auto& t : expr.impl_->terms)
    {
        terms.push_back({-t.coefficient, t.variable});
    }
    return Expression{makeImpl(-expr.impl_->constant, std::move(terms))};
}

Expression operator*(Expression expr, double scalar)
{
    if (scalar == 0.0)
    {
        return Expression{};
    }
    std::vector<Expression::Term> terms;
    terms.reserve(expr.impl_->terms.size());
    for (const auto& t : expr.impl_->terms)
    {
        terms.push_back({t.coefficient * scalar, t.variable});
    }
    return Expression{makeImpl(expr.impl_->constant * scalar, std::move(terms))};
}

Expression operator*(double scalar, Expression expr)
{
    return operator*(std::move(expr), scalar);
}

Expression operator/(Expression expr, double divisor)
{
    if (divisor == 0.0)
    {
        throw std::invalid_argument{"arrange: Expression division by zero"};
    }
    return operator*(std::move(expr), 1.0 / divisor);
}

}  // namespace arrange
