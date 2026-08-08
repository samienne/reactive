#pragma once

#include "constraint_impl.h"

#include <arrange/constraint.h>
#include <arrange/diff.h>
#include <arrange/id.h>
#include <arrange/strength.h>
#include <arrange/variable.h>

#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace arrange
{

inline constexpr double kSolverEpsilon = 1.0e-8;

inline bool isNearZero(double v) noexcept
{
    return v > -kSolverEpsilon && v < kSolverEpsilon;
}

class Symbol
{
public:
    enum class Type : std::uint8_t
    {
        invalid,
        external,
        slack,
        error,
        dummy
    };

    Symbol() noexcept = default;
    Symbol(Type type, std::uint32_t id) noexcept
        : id_(id)
        , type_(type)
    {
    }

    std::uint32_t id() const noexcept
    {
        return id_;
    }
    Type type() const noexcept
    {
        return type_;
    }

    bool operator==(const Symbol& o) const noexcept
    {
        return id_ == o.id_;
    }
    bool operator!=(const Symbol& o) const noexcept
    {
        return id_ != o.id_;
    }
    bool operator<(const Symbol& o) const noexcept
    {
        return id_ < o.id_;
    }

private:
    std::uint32_t id_ = 0;
    Type type_ = Type::invalid;
};

class Row
{
public:
    Row() = default;
    explicit Row(double constant)
        : constant_(constant)
    {
    }

    double constant() const noexcept
    {
        return constant_;
    }
    const std::map<Symbol, double>& cells() const noexcept
    {
        return cells_;
    }

    void setConstant(double v) noexcept
    {
        constant_ = v;
    }
    double addToConstant(double v) noexcept
    {
        constant_ += v;
        return constant_;
    }

    void insertSymbol(Symbol s, double coefficient);
    void insertRow(const Row& other, double coefficient = 1.0);
    void removeSymbol(Symbol s);
    void reverseSign();
    void solveForSymbol(Symbol s);
    void solveForSymbols(Symbol lhs, Symbol rhs);
    double coefficientFor(Symbol s) const;
    void substitute(Symbol s, const Row& row);

private:
    double constant_ = 0.0;
    std::map<Symbol, double> cells_;
};

struct Tag
{
    Symbol marker;
    Symbol other;
};

struct EditInfo
{
    Tag tag;
    Constraint constraint;
    double constant;
};

class SolverImpl
{
public:
    SolverImpl();
    SolverImpl(const SolverImpl& other);
    SolverImpl& operator=(const SolverImpl& other);
    SolverImpl(SolverImpl&&) noexcept = default;
    SolverImpl& operator=(SolverImpl&&) noexcept = default;
    ~SolverImpl() = default;

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
    Symbol nextSymbol(Symbol::Type t);
    Symbol getVarSymbol(const Variable& v);

    Row createRow(const Constraint& c, Tag& tagOut);
    Symbol chooseSubject(const Row& row, const Tag& tag) const;
    bool addWithArtificialVariable(const Row& row);

    void optimize(Row& objective);
    void dualOptimize();
    void substitute(Symbol s, const Row& row);

    Symbol getEnteringSymbol(const Row& objective) const;
    Symbol getLeavingSymbol(Symbol entering) const;
    Symbol getMarkerLeavingSymbol(Symbol marker) const;
    Symbol getDualEnteringSymbol(const Row& row) const;

    void removeConstraintEffects(const Constraint& c, const Tag& tag);
    void removeMarkerEffects(Symbol marker, const Strength& strength);

    // Tableau state
    std::map<Symbol, Row> rows_;
    std::unordered_map<Id, Symbol> vars_;
    std::unordered_map<const ConstraintImpl*, Tag> constraintTags_;
    std::unordered_map<Id, const ConstraintImpl*> idToImpl_;
    std::unordered_map<const ConstraintImpl*, Constraint> trackedConstraints_;
    std::unordered_map<Id, EditInfo> edits_;
    std::vector<Symbol> infeasibleRows_;
    Row objective_;
    std::unique_ptr<Row> artificial_;
    std::uint32_t symbolCounter_ = 1;
};

}  // namespace arrange
