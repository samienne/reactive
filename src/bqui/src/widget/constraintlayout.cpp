#include "constraintlayout.h"

#include <avg/transform.h>
#include <avg/vector.h>

#include <arrange/errors.h>
#include <arrange/solver.h>
#include <arrange/strength.h>

#include <utility>

namespace bqui::widget
{

namespace
{
    // The solver rides through the fold as accumulator state alongside the
    // snapshot it produced. Only the snapshot is mapped out and shared, so the
    // tableau is never copied at a shared node.
    struct SolveState
    {
        arrange::Solver solver;
        LayoutSolution solution;
    };

    float valueOf(LayoutSolution const& solution, arrange::Variable const& v)
    {
        auto it = solution.find(v.id());
        return it != solution.end() ? static_cast<float>(it->second) : 0.0f;
    }

    arrange::Constraint pin(arrange::Variable const& a, arrange::Variable const& b)
    {
        return arrange::Expression(a) == arrange::Expression(b);
    }
} // namespace

arrange::Expression BoxVariables::width() const
{
    return arrange::Expression(right) - arrange::Expression(left);
}

arrange::Expression BoxVariables::height() const
{
    return arrange::Expression(bottom) - arrange::Expression(top);
}

bq::signal::AnySignal<LayoutSolution> solveLayout(
        bq::signal::AnySignal<LayoutSpec> spec)
{
    return std::move(spec).withPrevious(
            [](SolveState state, LayoutSpec const& spec)
            {
                try
                {
                    state.solver.setConstraints(spec.constraints);
                }
                catch (arrange::Error const&)
                {
                    // An unsatisfiable update keeps the previous solution rather
                    // than tearing down the whole signal graph.
                }

                LayoutSolution solution;
                solution.reserve(spec.variables.size());
                for (auto const& variable : spec.variables)
                    solution.emplace(variable.id(), state.solver.valueOf(variable));

                state.solution = std::move(solution);
                return state;
            },
            SolveState{})
        .map([](SolveState const& state)
            {
                return state.solution;
            })
        .share();
}

avg::Obb readObb(LayoutSolution const& solution, BoxVariables const& box)
{
    float left = valueOf(solution, box.left);
    float top = valueOf(solution, box.top);
    float right = valueOf(solution, box.right);
    float bottom = valueOf(solution, box.bottom);

    return avg::Obb(
            avg::Vector2f(right - left, bottom - top),
            avg::Transform(avg::Vector2f(left, top)));
}

std::vector<arrange::Constraint> anchorConstraints(BoxVariables const& box,
        float left, float top, float right, float bottom)
{
    return {
        arrange::Expression(box.left) == arrange::Expression(left),
        arrange::Expression(box.top) == arrange::Expression(top),
        arrange::Expression(box.right) == arrange::Expression(right),
        arrange::Expression(box.bottom) == arrange::Expression(bottom),
    };
}

std::vector<arrange::Constraint> boxConstraints(BoxVariables const& container,
        std::vector<BoxVariables> const& children, Axis axis)
{
    std::vector<arrange::Constraint> out;

    for (std::size_t i = 0; i < children.size(); ++i)
    {
        BoxVariables const& child = children[i];
        bool first = i == 0;
        bool last = i + 1 == children.size();

        if (axis == Axis::y)
        {
            out.push_back(pin(child.left, container.left));
            out.push_back(pin(child.right, container.right));
            out.push_back(first ? pin(child.top, container.top)
                                : pin(child.top, children[i - 1].bottom));
            if (last)
                out.push_back(pin(child.bottom, container.bottom));
        }
        else
        {
            out.push_back(pin(child.top, container.top));
            out.push_back(pin(child.bottom, container.bottom));
            out.push_back(first ? pin(child.left, container.left)
                                : pin(child.left, children[i - 1].right));
            if (last)
                out.push_back(pin(child.right, container.right));
        }
    }

    return out;
}

std::vector<arrange::Constraint> stackConstraints(BoxVariables const& container,
        std::vector<BoxVariables> const& children)
{
    std::vector<arrange::Constraint> out;

    for (BoxVariables const& child : children)
    {
        out.push_back(pin(child.left, container.left));
        out.push_back(pin(child.top, container.top));
        out.push_back(pin(child.right, container.right));
        out.push_back(pin(child.bottom, container.bottom));
    }

    return out;
}

} // namespace bqui::widget
