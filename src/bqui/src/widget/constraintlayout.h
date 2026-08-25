#pragma once

#include "bqui/bquivisibility.h"

#include <bq/signal/signal.h>

#include <avg/obb.h>

#include <arrange/constraint.h>
#include <arrange/expression.h>
#include <arrange/id.h>
#include <arrange/variable.h>

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace bqui::widget
{
    /**
     * @brief The four edge variables bounding one widget's box in the shared
     * constraint system.
     *
     * A widget holds its BoxVariables for its whole lifetime, so the arrange
     * ids stay stable across solves and the solver's incremental diff can match
     * the widget's constraints from pass to pass.
     */
    struct BoxVariables
    {
        arrange::Variable left;
        arrange::Variable top;
        arrange::Variable right;
        arrange::Variable bottom;

        BQUI_EXPORT arrange::Expression width() const;
        BQUI_EXPORT arrange::Expression height() const;
    };

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
     * @brief Threads one arrange::Solver through the signal graph as a fold,
     * re-solving whenever @p spec changes, and yields the solved values.
     *
     * The solver is fold state — moved from update to update, never copied — and
     * the result is mapped down to the small value snapshot before it is shared,
     * so no reader ever copies the tableau. Build this once per window and share
     * the returned handle with every reader; a second instantiation would fork a
     * diverging solver.
     */
    BQUI_EXPORT bq::signal::AnySignal<LayoutSolution> solveLayout(
            bq::signal::AnySignal<LayoutSpec> spec);

    /**
     * @brief Reads a box's solved rectangle out of a solution as an avg::Obb.
     * Variables absent from the solution read as zero.
     */
    BQUI_EXPORT avg::Obb readObb(LayoutSolution const& solution,
            BoxVariables const& box);

    /**
     * @brief Pins a box to a fixed window-space rectangle (required).
     */
    BQUI_EXPORT std::vector<arrange::Constraint> anchorConstraints(
            BoxVariables const& box,
            float left, float top, float right, float bottom);

    enum class Axis
    {
        horizontal,
        vertical
    };

    /**
     * @brief Stacks @p children edge-to-edge inside @p container along @p axis.
     *
     * Consecutive children meet, the first and last touch the container's ends,
     * and the cross axis spans the container. The extent along @p axis is left
     * to each child's own size contribution.
     */
    BQUI_EXPORT std::vector<arrange::Constraint> boxConstraints(
            BoxVariables const& container,
            std::vector<BoxVariables> const& children, Axis axis);

    /**
     * @brief Overlays every child on @p container, giving each the container's
     * box.
     */
    BQUI_EXPORT std::vector<arrange::Constraint> stackConstraints(
            BoxVariables const& container,
            std::vector<BoxVariables> const& children);
} // namespace bqui::widget
