#pragma once

#include "bqui/widget/widget.h"

#include "bqui/bquivisibility.h"

#include <bq/signal/arraysignal.h>

#include <vector>

namespace bqui::widget
{
    /** @brief Lays a column of children out through the arrange solver.
     *
     * The container and every child carry a set of BoxVariables. The container
     * is anchored to the window-space rectangle it is realised at, the children
     * are stacked edge-to-edge along the vertical axis (boxConstraints()), and
     * each child's height is bounded to its SizeHint's [min, max] band. A child
     * settles at its natural size unless it is a filler (a larger max than
     * natural), in which case it shares the container's one stretch variable so
     * fillers split the leftover space equally. One arrange::Solver folds over
     * the whole spec (solveLayout()), and each child is placed at its solved
     * rectangle, flipped from the solver's top-down window space into the widget
     * tree's y-up coordinates. Every child is put through
     * modifier::handleGravity() so one that cannot use its whole slot is aligned
     * within it, exactly as layout() does.
     *
     * The array form follows a membership that changes; the vector form is the
     * fixed-list convenience over it.
     */
    BQUI_EXPORT AnyWidget solverVbox(bq::signal::ArraySignal<AnyWidget> widgets);

    /** @overload */
    BQUI_EXPORT AnyWidget solverVbox(std::vector<AnyWidget> widgets);
} // namespace bqui::widget
