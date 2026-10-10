#pragma once

#include <arrange/constraint.h>
#include <arrange/diff.h>
#include <arrange/strength.h>
#include <arrange/variable.h>

#include <memory>
#include <vector>

namespace arrange
{

class SolverImpl;

class Solver
{
public:
    Solver();
    ~Solver();

    Solver(const Solver& other);
    Solver& operator=(const Solver& other);

    Solver(Solver&& other) noexcept;
    Solver& operator=(Solver&& other) noexcept;

    void addConstraint(const Constraint& c);
    void removeConstraint(const Constraint& c);
    bool hasConstraint(const Constraint& c) const noexcept;

    ConstraintDiff setConstraints(const std::vector<Constraint>& desired);

    void addEditVariable(const Variable& v, Strength s);
    void removeEditVariable(const Variable& v);
    bool hasEditVariable(const Variable& v) const noexcept;
    void suggestValue(const Variable& v, double value);

    double valueOf(const Variable& v) const;
    bool contains(const Variable& v) const noexcept;

    void reset();

private:
    std::unique_ptr<SolverImpl> impl_;
};

}  // namespace arrange
