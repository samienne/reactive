#pragma once

#include <bq/signal/signal.h>

#include <arrange/constraint.h>
#include <arrange/id.h>
#include <arrange/variable.h>

#include <unordered_map>
#include <vector>

namespace bqui::widget
{
    /**
     * @brief One subtree's contribution to a solve: the constraints to apply
     * and the variables whose solved values must be read back.
     *
     * The solver exposes no variable iteration, so the variables to read travel
     * alongside the constraints that mention them.
     */
    struct LayoutSpec
    {
        std::vector<arrange::Constraint> constraints;
        std::vector<arrange::Variable> variables;
    };

    /**
     * @brief A solved layout: each read-back variable's value keyed by its
     * arrange id.
     */
    using LayoutSolution = std::unordered_map<arrange::Id, double>;

    /**
     * @brief A subtree's pure-solver constraints, accumulated onto its builder
     * and composed up through its containers, kept apart per axis so the two
     * disjoint solves each read only their own. Absent outside a pure-solver
     * region.
     */
    struct PureLayout
    {
        bq::signal::AnySignal<std::vector<LayoutSpec>> horizontal;
        bq::signal::AnySignal<std::vector<LayoutSpec>> vertical;
    };
} // namespace bqui::widget
