#pragma once

#include "constraintlayout.h"

#include "bqui/widget/instance.h"
#include "bqui/widget/widget.h"

#include "bqui/bquivisibility.h"

#include <bq/signal/arraysignal.h>

#include <avg/vector.h>

#include <vector>

namespace bqui::widget
{
    /**
     * @brief Wraps a subtree in one pure-solver region anchored to its own
     * size: the containers inside compose their constraints into a single
     * shared solve.
     *
     * A window root is solved through the same core (buildPureRegion()), so
     * applications never call it.
     */
    BQUI_EXPORT AnyWidget pureSolverRoot(AnyWidget content);

    /**
     * @brief Lays a column of children out in the enclosing region's solve.
     *
     * The children are stacked edge-to-edge along the vertical axis, each
     * settling within its own band; flexible children share the leftover space
     * in proportion to their flex. The container publishes the aggregate of its
     * children's bands as its own.
     *
     * The array form follows a membership that changes; the vector form is the
     * fixed-list convenience over it.
     */
    BQUI_EXPORT AnyWidget solverVbox(bq::signal::ArraySignal<AnyWidget> widgets);

    /**
     * @overload
     */
    BQUI_EXPORT AnyWidget solverVbox(std::vector<AnyWidget> widgets);

    /**
     * @brief Lays a row of children out in the enclosing region's solve.
     *
     * The horizontal counterpart of solverVbox().
     */
    BQUI_EXPORT AnyWidget solverHbox(bq::signal::ArraySignal<AnyWidget> widgets);

    /**
     * @overload
     */
    BQUI_EXPORT AnyWidget solverHbox(std::vector<AnyWidget> widgets);

    /**
     * @brief Overlays children in the enclosing region's solve.
     *
     * Every child is placed within the container's whole box on both axes, so
     * a child that cannot use the whole box settles at its natural size under
     * its own gravity while a filler grows to cover the container.
     */
    BQUI_EXPORT AnyWidget solverStack(bq::signal::ArraySignal<AnyWidget> widgets);

    /**
     * @overload
     */
    BQUI_EXPORT AnyWidget solverStack(std::vector<AnyWidget> widgets);

    /**
     * @brief Lays a uniform grid out in the enclosing region's solve.
     *
     * The container is split into @p columns equal-width columns and @p rows
     * equal-height rows (gridLines()); each child is placed within the box of
     * the cell @p cells names for it, so it settles under its gravity where it
     * cannot use the whole cell. @p cells is parallel to @p widgets.
     */
    BQUI_EXPORT AnyWidget solverUniformGrid(std::vector<AnyWidget> widgets,
            std::vector<GridCell> cells, unsigned int columns,
            unsigned int rows);

    /**
     * @brief The band a region publishes outward: its outermost box's width
     * band (phase 1) and its height band at the width the region is solved at
     * (phase 2).
     */
    struct RegionBand
    {
        Constraints width;
        Constraints height;
    };

    /**
     * @brief A region built at a size: its placed element and the band it
     * publishes outward.
     */
    struct PureRegion
    {
        AnyElement element;
        bq::signal::AnySignal<RegionBand> band;
    };

    /**
     * @brief Solves @p content as a self-contained region anchored to @p size,
     * returning the built element together with the content's band.
     *
     * The band is what the region's owner would size its boundary from; the
     * solve inside does not depend on it beyond the content's own constraints.
     */
    BQUI_EXPORT PureRegion buildPureRegion(AnyWidget const& content,
            bq::signal::AnySignal<avg::Vector2f> size,
            BuildParams const& params);

    /**
     * @brief Solves @p content as a self-contained pure-solver region anchored to
     * @p size and returns its built, placed instance.
     *
     * The reusable core of a firewall. It reads @p content's composed pure
     * descriptor, anchors its outermost box to @p size, runs the two disjoint
     * per-axis region solves, and threads the combined solution into the build
     * so every container inside places its children against a real solution on
     * the first evaluate. pureSolverRoot() anchors to the window; bin() anchors
     * to the clipped content size, so content size dies at the boundary.
     */
    bq::signal::AnySignal<widget::Instance> solvePureRegionAtSize(
            AnyWidget const& content,
            bq::signal::AnySignal<avg::Vector2f> size,
            BuildParams const& params);

} // namespace bqui::widget
