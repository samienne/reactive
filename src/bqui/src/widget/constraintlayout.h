#pragma once

#include "bqui/bquivisibility.h"

#include "bqui/sizehint.h"
#include "bqui/widget/boxvariables.h"

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
    /** @brief One subtree's contribution to a solve: the constraints to apply
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

    /** @brief A solved layout: each read-back variable's value keyed by its
     * arrange id. */
    using LayoutSolution = std::unordered_map<arrange::Id, double>;

    /** @brief Threads one arrange::Solver through the signal graph as a fold,
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

    /** @brief Reads a box's solved rectangle out of a solution as an avg::Obb.
     * Variables absent from the solution read as zero. */
    BQUI_EXPORT avg::Obb readObb(LayoutSolution const& solution,
            BoxVariables const& box);

    /** @brief Pins a box to a fixed window-space rectangle (required). */
    BQUI_EXPORT std::vector<arrange::Constraint> anchorConstraints(
            BoxVariables const& box,
            float left, float top, float right, float bottom);

    /** @brief Stacks @p children edge-to-edge inside @p container along @p axis.
     *
     * Consecutive children meet, the first touches the container's leading end,
     * and the cross axis spans the container. The trailing end is capped at the
     * container's end (required, so a stack never overflows) and pulled to it
     * only weakly, so a child free to grow fills the container while firmer
     * content packs against the leading end and leaves the slack as a gap. The
     * extent along @p axis is otherwise left to each child's own size
     * contribution. @p axis is the shared bqui::Axis: Axis::y stacks top to
     * bottom, Axis::x left to right.
     */
    BQUI_EXPORT std::vector<arrange::Constraint> boxConstraints(
            BoxVariables const& container,
            std::vector<BoxVariables> const& children, Axis axis);

    /** @brief Overlays every child on @p container, giving each the container's
     * box. */
    BQUI_EXPORT std::vector<arrange::Constraint> stackConstraints(
            BoxVariables const& container,
            std::vector<BoxVariables> const& children);
} // namespace bqui::widget
