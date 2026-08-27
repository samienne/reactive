#pragma once

#include "constraintlayout.h"

#include "bqui/widget/widget.h"

#include "bqui/bquivisibility.h"

#include <bq/signal/arraysignal.h>

#include <cstddef>
#include <vector>

namespace bqui::widget
{
    /**
     * @brief How a row aligns its children across its cross (vertical) axis.
     *
     * @c fill is the default: each child spans the row's height and settles
     * under its own gravity. @c baseline instead keeps each child at its
     * natural height and lines the children up on a shared baseline, so text of
     * different sizes sits on one line.
     */
    enum class CrossAlign
    {
        fill,
        baseline
    };

    /**
     * @brief Lays a column of children out through the arrange solver.
     *
     * The container and every child carry a set of BoxVariables. The container
     * is anchored to the window-space rectangle it is realised at, the children
     * are stacked edge-to-edge along the vertical axis (boxConstraints()), and
     * each child's height is bounded to its SizeHint's [min, max] band. A child
     * settles at its natural size unless it is a filler (a positive grow
     * weight), in which case it shares the container's one stretch variable so
     * fillers split the leftover space in proportion to their weights. One
     * arrange::Solver folds over
     * the whole spec (solveLayout()), and each child is placed at its solved
     * rectangle, flipped from the solver's top-down window space into the widget
     * tree's y-up coordinates. Across the cross axis each child is sized and
     * positioned within the container by placeInSlot(), so one that cannot use
     * its whole slot settles under its gravity within it — all in the one solve.
     *
     * The array form follows a membership that changes; the vector form is the
     * fixed-list convenience over it.
     */
    /**
     * @brief Wraps a subtree in one firewall region: the containers inside emit
     * their constraints into a single shared solve instead of each folding its
     * own.
     *
     * The wrapper owns the region's makeInput/solve cycle. It seeds the
     * down-channel (LayoutSolutionTag) and the up-channel (RegionCollectorTag)
     * its participating containers read, folds the fragments they contribute into
     * one solveLayout (layoutRegion), and ties the result back to the solution
     * every box reads its geometry from. A container that finds no region around
     * it runs the per-container path unchanged, so this is the opt-in and the old
     * path stays the default.
     */
    BQUI_EXPORT AnyWidget regionRoot(AnyWidget content);

    BQUI_EXPORT AnyWidget solverVbox(bq::signal::ArraySignal<AnyWidget> widgets);

    /**
     * @overload
     */
    BQUI_EXPORT AnyWidget solverVbox(std::vector<AnyWidget> widgets);

    /**
     * @brief Lays a row of children out through the arrange solver.
     *
     * The horizontal counterpart of solverVbox(): the same spec and the same
     * solve, with the children stacked edge-to-edge along the horizontal axis
     * and each child's width, rather than its height, bounded to its SizeHint's
     * band.
     */
    BQUI_EXPORT AnyWidget solverHbox(bq::signal::ArraySignal<AnyWidget> widgets);

    /**
     * @overload
     */
    BQUI_EXPORT AnyWidget solverHbox(std::vector<AnyWidget> widgets);

    /**
     * @brief Lays a row out through the arrange solver, its children aligned on
     * a shared baseline.
     *
     * Like solverHbox() along the main axis, but across the cross axis each
     * child that reports a firstBaseline keeps its natural height and is placed
     * so its baseline meets the row's shared baseline line; a child without a
     * baseline falls back to spanning the row as in a plain hbox. The row
     * reports its own firstBaseline upward (accumulateBaselineRowHints()), so
     * nesting baseline rows compose.
     */
    BQUI_EXPORT AnyWidget baselineHbox(bq::signal::ArraySignal<AnyWidget> widgets);

    /**
     * @overload
     */
    BQUI_EXPORT AnyWidget baselineHbox(std::vector<AnyWidget> widgets);

    /**
     * @brief Overlays children through the arrange solver.
     *
     * Every child is placed within the container's whole box by placeInSlot()
     * on both axes, so a child that cannot use the whole box settles at its
     * natural size under its own gravity while a filler grows to cover the
     * container. The container reports the per-axis maximum of its children's
     * size hints upward, the overlay's own size band.
     */
    BQUI_EXPORT AnyWidget solverStack(bq::signal::ArraySignal<AnyWidget> widgets);

    /**
     * @overload
     */
    BQUI_EXPORT AnyWidget solverStack(std::vector<AnyWidget> widgets);

    /**
     * @brief Lays a uniform grid out through the arrange solver.
     *
     * The container is split into @p columns equal-width columns and @p rows
     * equal-height rows (gridLines()); each child is placed within the box of
     * the cell @p cells names for it by placeInSlot(), so it settles under its
     * gravity where it cannot use the whole cell. @p cells is parallel to
     * @p widgets.
     */
    BQUI_EXPORT AnyWidget solverUniformGrid(std::vector<AnyWidget> widgets,
            std::vector<GridCell> cells, unsigned int columns,
            unsigned int rows);

    /**
     * @brief Diagnostic: the number of entries provideParam<ResolvedGuides>()
     * reads from @p params when it is instantiated inside the bqui library.
     *
     * A container reads the inherited resolved-guide map with
     * provideParam<ResolvedGuides>() compiled into this library, while a caller
     * that injects the map with setParams<ResolvedGuides> compiles that in its
     * own binary. Setting the param in one binary and reading the count here
     * tells a map that crossed the library boundary intact apart from one the
     * library-side BuildParams lookup missed and defaulted to empty.
     */
    BQUI_EXPORT std::size_t resolvedGuideParamCount(BuildParams const& params);
} // namespace bqui::widget
