#include "constraint_impl.h"
#include "solver_impl.h"

#include <arrange/diff.h>
#include <arrange/errors.h>
#include <arrange/solver.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

namespace arrange
{

// --- Row --------------------------------------------------------------------

void Row::insertSymbol(Symbol s, double coefficient)
{
    auto [it, inserted] = cells_.try_emplace(s, coefficient);
    if (!inserted)
    {
        it->second += coefficient;
        if (isNearZero(it->second))
        {
            cells_.erase(it);
        }
    }
}

void Row::insertRow(const Row& other, double coefficient)
{
    constant_ += other.constant_ * coefficient;
    for (const auto& [s, v] : other.cells_)
    {
        insertSymbol(s, v * coefficient);
    }
}

void Row::removeSymbol(Symbol s)
{
    cells_.erase(s);
}

void Row::reverseSign()
{
    constant_ = -constant_;
    for (auto& [s, v] : cells_)
    {
        v = -v;
    }
}

void Row::solveForSymbol(Symbol s)
{
    // Row is: 0 = constant_ + sum(coeff_i * sym_i) + coeff_s * s
    // Solve: s = -(constant_ + sum(coeff_i * sym_i)) / coeff_s
    auto it = cells_.find(s);
    const double coeff = -1.0 / it->second;
    cells_.erase(it);
    constant_ *= coeff;
    for (auto& [sym, v] : cells_)
    {
        v *= coeff;
    }
}

void Row::solveForSymbols(Symbol lhs, Symbol rhs)
{
    insertSymbol(lhs, -1.0);
    solveForSymbol(rhs);
}

double Row::coefficientFor(Symbol s) const
{
    auto it = cells_.find(s);
    return it == cells_.end() ? 0.0 : it->second;
}

void Row::substitute(Symbol s, const Row& row)
{
    auto it = cells_.find(s);
    if (it == cells_.end())
    {
        return;
    }
    const double coeff = it->second;
    cells_.erase(it);
    insertRow(row, coeff);
}

// --- SolverImpl helpers -----------------------------------------------------

SolverImpl::SolverImpl() = default;

SolverImpl::SolverImpl(const SolverImpl& other)
    : rows_(other.rows_)
    , vars_(other.vars_)
    , constraintTags_(other.constraintTags_)
    , idToImpl_(other.idToImpl_)
    , trackedConstraints_(other.trackedConstraints_)
    , edits_(other.edits_)
    , infeasibleRows_(other.infeasibleRows_)
    , objective_(other.objective_)
    , artificial_(nullptr)
    , symbolCounter_(other.symbolCounter_)
{
}

SolverImpl& SolverImpl::operator=(const SolverImpl& other)
{
    if (this != &other)
    {
        rows_ = other.rows_;
        vars_ = other.vars_;
        constraintTags_ = other.constraintTags_;
        idToImpl_ = other.idToImpl_;
        trackedConstraints_ = other.trackedConstraints_;
        edits_ = other.edits_;
        infeasibleRows_ = other.infeasibleRows_;
        objective_ = other.objective_;
        artificial_.reset();
        symbolCounter_ = other.symbolCounter_;
    }
    return *this;
}

Symbol SolverImpl::nextSymbol(Symbol::Type t)
{
    return Symbol{t, symbolCounter_++};
}

Symbol SolverImpl::getVarSymbol(const Variable& v)
{
    auto it = vars_.find(v.id());
    if (it != vars_.end())
    {
        return it->second;
    }
    Symbol sym = nextSymbol(Symbol::Type::external);
    vars_.emplace(v.id(), sym);
    return sym;
}

Row SolverImpl::createRow(const Constraint& c, Tag& tagOut)
{
    const Expression& expr = c.impl_->expression;
    Row row(expr.constant());
    for (std::size_t i = 0; i < expr.termCount(); ++i)
    {
        const auto t = expr.term(i);
        if (isNearZero(t.coefficient))
        {
            continue;
        }
        Symbol sym = getVarSymbol(t.variable);
        auto it = rows_.find(sym);
        if (it != rows_.end())
        {
            row.insertRow(it->second, t.coefficient);
        }
        else
        {
            row.insertSymbol(sym, t.coefficient);
        }
    }

    const Strength strength = c.impl_->strength;
    const Relation rel = c.impl_->relation;

    switch (rel)
    {
    case Relation::le:
    case Relation::ge:
    {
        const double coeff = (rel == Relation::le) ? 1.0 : -1.0;
        Symbol slack = nextSymbol(Symbol::Type::slack);
        tagOut.marker = slack;
        row.insertSymbol(slack, coeff);
        if (!strength.isRequired())
        {
            Symbol error = nextSymbol(Symbol::Type::error);
            tagOut.other = error;
            row.insertSymbol(error, -coeff);
            objective_.insertSymbol(error, strength.value());
        }
        break;
    }
    case Relation::eq:
    {
        if (!strength.isRequired())
        {
            Symbol errPlus = nextSymbol(Symbol::Type::error);
            Symbol errMinus = nextSymbol(Symbol::Type::error);
            tagOut.marker = errPlus;
            tagOut.other = errMinus;
            row.insertSymbol(errPlus, -1.0);
            row.insertSymbol(errMinus, 1.0);
            objective_.insertSymbol(errPlus, strength.value());
            objective_.insertSymbol(errMinus, strength.value());
        }
        else
        {
            Symbol dummy = nextSymbol(Symbol::Type::dummy);
            tagOut.marker = dummy;
            row.insertSymbol(dummy, 1.0);
        }
        break;
    }
    }

    if (row.constant() < 0.0)
    {
        row.reverseSign();
    }
    return row;
}

Symbol SolverImpl::chooseSubject(const Row& row, const Tag& tag) const
{
    for (const auto& [sym, coeff] : row.cells())
    {
        if (sym.type() == Symbol::Type::external)
        {
            return sym;
        }
    }
    if (tag.marker.type() == Symbol::Type::slack || tag.marker.type() == Symbol::Type::error)
    {
        if (row.coefficientFor(tag.marker) < 0.0)
        {
            return tag.marker;
        }
    }
    if (tag.other.type() == Symbol::Type::slack || tag.other.type() == Symbol::Type::error)
    {
        if (row.coefficientFor(tag.other) < 0.0)
        {
            return tag.other;
        }
    }
    return Symbol{};
}

bool SolverImpl::addWithArtificialVariable(const Row& rowIn)
{
    Symbol art = nextSymbol(Symbol::Type::slack);
    rows_.emplace(art, rowIn);
    artificial_ = std::make_unique<Row>(rowIn);

    optimize(*artificial_);
    const bool success = isNearZero(artificial_->constant());
    artificial_.reset();

    auto itArt = rows_.find(art);
    if (itArt != rows_.end())
    {
        Row artRow = std::move(itArt->second);
        rows_.erase(itArt);
        if (!artRow.cells().empty())
        {
            Symbol entering{};
            for (const auto& [sym, coeff] : artRow.cells())
            {
                if (sym.type() != Symbol::Type::dummy)
                {
                    entering = sym;
                    break;
                }
            }
            if (entering.type() == Symbol::Type::invalid)
            {
                return false;
            }
            artRow.solveForSymbols(art, entering);
            substitute(entering, artRow);
            rows_.emplace(entering, std::move(artRow));
        }
    }
    for (auto& [basic, r] : rows_)
    {
        r.removeSymbol(art);
    }
    objective_.removeSymbol(art);
    return success;
}

void SolverImpl::substitute(Symbol s, const Row& row)
{
    for (auto& [basic, r] : rows_)
    {
        r.substitute(s, row);
        if (basic.type() != Symbol::Type::external && r.constant() < 0.0)
        {
            infeasibleRows_.push_back(basic);
        }
    }
    objective_.substitute(s, row);
    if (artificial_)
    {
        artificial_->substitute(s, row);
    }
}

Symbol SolverImpl::getEnteringSymbol(const Row& objective) const
{
    for (const auto& [sym, coeff] : objective.cells())
    {
        if (sym.type() != Symbol::Type::dummy && coeff < -kSolverEpsilon)
        {
            return sym;
        }
    }
    return Symbol{};
}

Symbol SolverImpl::getLeavingSymbol(Symbol entering) const
{
    double ratio = std::numeric_limits<double>::infinity();
    Symbol leaving{};
    for (const auto& [basic, row] : rows_)
    {
        if (basic.type() == Symbol::Type::external)
        {
            continue;
        }
        const double coeff = row.coefficientFor(entering);
        if (coeff < -kSolverEpsilon)
        {
            const double r = -row.constant() / coeff;
            if (r < ratio)
            {
                ratio = r;
                leaving = basic;
            }
        }
    }
    return leaving;
}

Symbol SolverImpl::getMarkerLeavingSymbol(Symbol marker) const
{
    const double inf = std::numeric_limits<double>::infinity();
    double r1 = inf;
    double r2 = inf;
    Symbol first{};
    Symbol second{};
    Symbol third{};
    for (const auto& [basic, row] : rows_)
    {
        const double c = row.coefficientFor(marker);
        if (c == 0.0)
        {
            continue;
        }
        if (basic.type() == Symbol::Type::external)
        {
            third = basic;
        }
        else if (c < 0.0)
        {
            const double r = -row.constant() / c;
            if (r < r1)
            {
                r1 = r;
                first = basic;
            }
        }
        else
        {
            const double r = row.constant() / c;
            if (r < r2)
            {
                r2 = r;
                second = basic;
            }
        }
    }
    if (first.type() != Symbol::Type::invalid)
        return first;
    if (second.type() != Symbol::Type::invalid)
        return second;
    return third;
}

Symbol SolverImpl::getDualEnteringSymbol(const Row& row) const
{
    double ratio = std::numeric_limits<double>::infinity();
    Symbol entering{};
    for (const auto& [sym, coeff] : row.cells())
    {
        if (coeff > 0.0 && sym.type() != Symbol::Type::dummy)
        {
            const double objCoeff = objective_.coefficientFor(sym);
            const double r = objCoeff / coeff;
            if (r < ratio)
            {
                ratio = r;
                entering = sym;
            }
        }
    }
    return entering;
}

void SolverImpl::optimize(Row& objective)
{
    while (true)
    {
        Symbol entering = getEnteringSymbol(objective);
        if (entering.type() == Symbol::Type::invalid)
        {
            return;
        }
        Symbol leaving = getLeavingSymbol(entering);
        if (leaving.type() == Symbol::Type::invalid)
        {
            throw Error{"arrange: solver internal error — unbounded objective"};
        }
        auto it = rows_.find(leaving);
        Row row = std::move(it->second);
        rows_.erase(it);
        row.solveForSymbols(leaving, entering);
        substitute(entering, row);
        objective.substitute(entering, row);
        rows_.emplace(entering, std::move(row));
    }
}

void SolverImpl::dualOptimize()
{
    while (!infeasibleRows_.empty())
    {
        Symbol leaving = infeasibleRows_.back();
        infeasibleRows_.pop_back();
        auto it = rows_.find(leaving);
        if (it == rows_.end() || it->second.constant() >= 0.0)
        {
            continue;
        }
        Row row = std::move(it->second);
        rows_.erase(it);
        Symbol entering = getDualEnteringSymbol(row);
        if (entering.type() == Symbol::Type::invalid)
        {
            throw Error{"arrange: solver internal error — dual optimize failed"};
        }
        row.solveForSymbols(leaving, entering);
        substitute(entering, row);
        rows_.emplace(entering, std::move(row));
    }
}

void SolverImpl::removeMarkerEffects(Symbol marker, const Strength& strength)
{
    objective_.insertSymbol(marker, -strength.value());
}

void SolverImpl::removeConstraintEffects(const Constraint& c, const Tag& tag)
{
    const Strength s = c.strength();
    if (!s.isRequired())
    {
        // Error variables were added with +strength to the objective.
        if (tag.marker.type() == Symbol::Type::error)
        {
            removeMarkerEffects(tag.marker, s);
        }
        if (tag.other.type() == Symbol::Type::error)
        {
            removeMarkerEffects(tag.other, s);
        }
    }
}

// --- Public API -------------------------------------------------------------

void SolverImpl::addConstraint(const Constraint& c)
{
    const Id id = c.id();
    if (id != nullId)
    {
        if (idToImpl_.find(id) != idToImpl_.end())
        {
            throw DuplicateConstraint{"arrange: constraint id already present"};
        }
    }
    if (constraintTags_.find(c.impl_.get()) != constraintTags_.end())
    {
        throw DuplicateConstraint{"arrange: constraint already present"};
    }

    Tag tag;
    Row row = createRow(c, tag);
    Symbol subject = chooseSubject(row, tag);

    if (subject.type() == Symbol::Type::invalid && row.cells().empty())
    {
        // All-dummy row. Valid only if constant is zero.
        if (isNearZero(row.constant()))
        {
            subject = tag.marker;
        }
        else
        {
            throw UnsatisfiableConstraint{"arrange: required constraint unsatisfiable"};
        }
    }

    if (subject.type() == Symbol::Type::invalid)
    {
        if (!addWithArtificialVariable(row))
        {
            throw UnsatisfiableConstraint{"arrange: required constraint unsatisfiable"};
        }
    }
    else
    {
        row.solveForSymbol(subject);
        substitute(subject, row);
        rows_.emplace(subject, std::move(row));
    }

    constraintTags_.emplace(c.impl_.get(), tag);
    trackedConstraints_.emplace(c.impl_.get(), c);
    if (id != nullId)
    {
        idToImpl_.emplace(id, c.impl_.get());
    }
    optimize(objective_);
}

void SolverImpl::removeConstraint(const Constraint& c)
{
    const Id id = c.id();
    const ConstraintImpl* impl = nullptr;
    auto tagIt = constraintTags_.end();

    if (id == nullId)
    {
        tagIt = constraintTags_.find(c.impl_.get());
        if (tagIt == constraintTags_.end())
        {
            throw UnknownConstraint{"arrange: constraint not in solver"};
        }
        impl = c.impl_.get();
    }
    else
    {
        auto idIt = idToImpl_.find(id);
        if (idIt == idToImpl_.end())
        {
            throw UnknownConstraint{"arrange: constraint id not in solver"};
        }
        impl = idIt->second;
        tagIt = constraintTags_.find(impl);
    }

    const Tag tag = tagIt->second;
    constraintTags_.erase(tagIt);
    trackedConstraints_.erase(impl);
    if (id != nullId)
    {
        idToImpl_.erase(id);
    }

    // Rebuild the Constraint pointer to its impl through any copy we have;
    // we only need its strength/relation for removeMarkerEffects.
    // We pass `c` directly — its strength matches the original added one
    // because the impl is shared across copies.
    removeConstraintEffects(c, tag);

    // Remove the marker from the tableau.
    auto rowIt = rows_.find(tag.marker);
    if (rowIt != rows_.end())
    {
        rows_.erase(rowIt);
    }
    else
    {
        Symbol leaving = getMarkerLeavingSymbol(tag.marker);
        if (leaving.type() == Symbol::Type::invalid)
        {
            throw Error{"arrange: solver internal error — cannot remove marker"};
        }
        auto lit = rows_.find(leaving);
        Row row = std::move(lit->second);
        rows_.erase(lit);
        row.solveForSymbols(leaving, tag.marker);
        substitute(tag.marker, row);
    }
    optimize(objective_);
}

bool SolverImpl::hasConstraint(const Constraint& c) const noexcept
{
    return constraintTags_.find(c.impl_.get()) != constraintTags_.end();
}

ConstraintDiff SolverImpl::setConstraints(const std::vector<Constraint>& desired)
{
    std::vector<Constraint> current;
    current.reserve(trackedConstraints_.size());
    for (const auto& [impl, c] : trackedConstraints_)
    {
        current.push_back(c);
    }
    ConstraintDiff diff = diffConstraints(current, desired);

    // Strong exception safety: snapshot and restore on throw.
    SolverImpl backup = *this;
    try
    {
        for (const auto& c : diff.removed)
        {
            removeConstraint(c);
        }
        for (const auto& c : diff.added)
        {
            addConstraint(c);
        }
    }
    catch (...)
    {
        *this = std::move(backup);
        throw;
    }
    return diff;
}

void SolverImpl::addEditVariable(const Variable& v, Strength s)
{
    if (s.isRequired())
    {
        throw BadStrength{"arrange: edit variable strength must not be required"};
    }
    if (edits_.find(v.id()) != edits_.end())
    {
        throw DuplicateEditVariable{"arrange: edit variable already present"};
    }
    Constraint c = (Expression{v} == 0.0) | s;
    addConstraint(c);
    auto it = constraintTags_.find(c.impl_.get());
    EditInfo info{it->second, c, 0.0};
    edits_.emplace(v.id(), std::move(info));
}

void SolverImpl::removeEditVariable(const Variable& v)
{
    auto it = edits_.find(v.id());
    if (it == edits_.end())
    {
        throw UnknownEditVariable{"arrange: edit variable not present"};
    }
    Constraint c = it->second.constraint;
    edits_.erase(it);
    removeConstraint(c);
}

bool SolverImpl::hasEditVariable(const Variable& v) const noexcept
{
    return edits_.find(v.id()) != edits_.end();
}

void SolverImpl::suggestValue(const Variable& v, double value)
{
    auto it = edits_.find(v.id());
    if (it == edits_.end())
    {
        throw UnknownEditVariable{"arrange: edit variable not present"};
    }
    EditInfo& info = it->second;
    const double delta = value - info.constant;
    info.constant = value;

    // Update plus error row
    auto rit = rows_.find(info.tag.marker);
    if (rit != rows_.end())
    {
        if (rit->second.addToConstant(-delta) < 0.0)
        {
            infeasibleRows_.push_back(info.tag.marker);
        }
    }
    else
    {
        rit = rows_.find(info.tag.other);
        if (rit != rows_.end())
        {
            if (rit->second.addToConstant(delta) < 0.0)
            {
                infeasibleRows_.push_back(info.tag.other);
            }
        }
        else
        {
            for (auto& [basic, row] : rows_)
            {
                const double coeff = row.coefficientFor(info.tag.marker);
                if (coeff != 0.0)
                {
                    if (row.addToConstant(delta * coeff) < 0.0
                        && basic.type() != Symbol::Type::external)
                    {
                        infeasibleRows_.push_back(basic);
                    }
                }
            }
        }
    }
    dualOptimize();
}

double SolverImpl::valueOf(const Variable& v) const
{
    auto it = vars_.find(v.id());
    if (it == vars_.end())
    {
        return 0.0;
    }
    auto rit = rows_.find(it->second);
    if (rit == rows_.end())
    {
        return 0.0;
    }
    return rit->second.constant();
}

bool SolverImpl::contains(const Variable& v) const noexcept
{
    return vars_.find(v.id()) != vars_.end();
}

void SolverImpl::reset()
{
    rows_.clear();
    vars_.clear();
    constraintTags_.clear();
    idToImpl_.clear();
    trackedConstraints_.clear();
    edits_.clear();
    infeasibleRows_.clear();
    objective_ = Row{};
    artificial_.reset();
    symbolCounter_ = 1;
}

// --- Solver (public wrapper) -----------------------------------------------

Solver::Solver()
    : impl_(std::make_unique<SolverImpl>())
{
}

Solver::~Solver() = default;

Solver::Solver(const Solver& other)
    : impl_(std::make_unique<SolverImpl>(*other.impl_))
{
}

Solver& Solver::operator=(const Solver& other)
{
    if (this != &other)
    {
        impl_ = std::make_unique<SolverImpl>(*other.impl_);
    }
    return *this;
}

Solver::Solver(Solver&& other) noexcept = default;
Solver& Solver::operator=(Solver&& other) noexcept = default;

void Solver::addConstraint(const Constraint& c)
{
    impl_->addConstraint(c);
}

void Solver::removeConstraint(const Constraint& c)
{
    impl_->removeConstraint(c);
}

bool Solver::hasConstraint(const Constraint& c) const noexcept
{
    return impl_->hasConstraint(c);
}

ConstraintDiff Solver::setConstraints(const std::vector<Constraint>& desired)
{
    return impl_->setConstraints(desired);
}

void Solver::addEditVariable(const Variable& v, Strength s)
{
    impl_->addEditVariable(v, s);
}

void Solver::removeEditVariable(const Variable& v)
{
    impl_->removeEditVariable(v);
}

bool Solver::hasEditVariable(const Variable& v) const noexcept
{
    return impl_->hasEditVariable(v);
}

void Solver::suggestValue(const Variable& v, double value)
{
    impl_->suggestValue(v, value);
}

double Solver::valueOf(const Variable& v) const
{
    return impl_->valueOf(v);
}

bool Solver::contains(const Variable& v) const noexcept
{
    return impl_->contains(v);
}

void Solver::reset()
{
    impl_->reset();
}

}  // namespace arrange
