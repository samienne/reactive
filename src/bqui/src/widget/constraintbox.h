#pragma once

#include "bqui/widget/widget.h"

#include "bqui/bquivisibility.h"

#include <vector>

namespace bqui::widget
{
    /** @brief Lays a column of children out through the arrange solver.
     *
     * The container and every child carry a set of BoxVariables. The container
     * is anchored to the window-space rectangle it is realised at, the children
     * are stacked edge-to-edge along the vertical axis (boxConstraints()), and
     * each child's preferred and minimum height enter as a strong equality and
     * a required lower bound read from its SizeHint. One arrange::Solver folds
     * over the whole spec (solveLayout()), and each child is placed at its
     * solved rectangle, flipped from the solver's top-down window space into the
     * widget tree's y-up coordinates.
     *
     * This is the first container wired onto the solver; the fixed-membership
     * form is enough to prove the integration end to end.
     */
    BQUI_EXPORT AnyWidget solverVbox(std::vector<AnyWidget> widgets);
} // namespace bqui::widget
