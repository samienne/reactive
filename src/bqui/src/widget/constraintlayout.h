#pragma once

#include "bqui/bquivisibility.h"

#include "bqui/sizehint.h"
#include "bqui/widget/boxvariables.h"
#include "bqui/widget/guide.h"
#include "bqui/widget/resolvedguides.h"

#include <bq/signal/constant.h>
#include <bq/signal/sharedvector.h>
#include <bq/signal/signal.h>

#include <avg/obb.h>

#include <arrange/constraint.h>
#include <arrange/expression.h>
#include <arrange/id.h>
#include <arrange/variable.h>

#include <cstddef>
#include <map>
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
     * @brief The down-channel entry a widget in a firewall region reads to learn
     * the region's solved geometry.
     *
     * A region owner runs one solve spanning every container in the region and
     * provides the shared solution here, so each participating box reads its own
     * obb out of the one solution (readObb) instead of running a per-container
     * solve. This is the region counterpart of ResolvedGuides: the guide map
     * carries resolved positions down, this carries the whole solved tableau
     * down. The default is empty, which is what a box outside any region reads
     * and reads back as a zero obb.
     */
    struct LayoutSolutionTag
    {
        using type = LayoutSolution;

        static bq::signal::AnySignal<LayoutSolution> getDefaultValue()
        {
            return bq::signal::constant(LayoutSolution());
        }
    };

    /**
     * @brief The up-channel a region owner threads down for its participating
     * containers to append their per-container spec fragments to.
     *
     * The presence of this entry is what marks a subtree as inside a region: a
     * container that finds it appends its own fragment and reads its geometry
     * from the shared LayoutSolutionTag rather than running its own solve. The
     * region owner (regionRoot) holds the other copy of the same SharedVector and
     * folds its contents into the one region solve. Fragments are appended as the
     * subtree builds, so the collection is exported as a signal that settles a
     * pass behind the build, like any change-driven input.
     */
    struct RegionCollectorTag
    {
        using type = bq::signal::SharedVector<
            bq::signal::AnySignal<LayoutSpec>>;

        static bq::signal::AnySignal<type> getDefaultValue()
        {
            return bq::signal::constant(type());
        }
    };

    /**
     * @brief Whether the container reading it is the outermost in its region and
     * so must anchor the region's coordinate origin.
     *
     * The region owner seeds this true; the outermost participating container
     * anchors its box to its assigned rectangle and seeds this false for its
     * descendants, which are placed by their parents' tiling and must not anchor
     * a second origin into the shared tableau.
     */
    struct RegionAnchorTag
    {
        using type = bool;

        static bq::signal::AnySignal<bool> getDefaultValue()
        {
            return bq::signal::constant(true);
        }
    };

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
     * @brief The single solve owning a firewall region: concatenates the
     * region's collected per-container spec fragments into one tableau and
     * solves them together, yielding the shared solution every participating box
     * reads its obb from.
     *
     * Where solveLayout() folds one container's spec, this is the region owner:
     * it gathers the fragments a nested set of containers contribute — each
     * container's own constraints, anchored into the one shared coordinate space
     * rather than a per-container local one — and runs a single solve across the
     * whole region, so constraints couple across container levels. The fragment
     * list is the up-channel a region collects; the returned solution is the
     * down-channel it provides through LayoutSolutionTag.
     */
    BQUI_EXPORT bq::signal::AnySignal<LayoutSolution> layoutRegion(
            bq::signal::AnySignal<std::vector<LayoutSpec>> fragments);

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

    /**
     * @brief Tiles @p children edge-to-edge inside @p container along @p axis.
     *
     * Consecutive children meet, the first touches the container's leading end,
     * and the trailing end is pulled to the container's end only weakly, so a
     * child free to grow fills the container while firmer content packs against
     * the leading end and leaves the slack as a gap. Nothing caps the trailing
     * end, so an over-full box overflows rather than squeezing. The extent along
     * @p axis is otherwise left to each child's own size contribution. @p axis
     * is the shared bqui::Axis: Axis::y stacks top to bottom, Axis::x left to
     * right.
     *
     * Only the main axis is constrained. The cross axis is left free for
     * placeInSlot() (or a baseline pass) to size and position each child within
     * the container's cross extent.
     */
    BQUI_EXPORT std::vector<arrange::Constraint> boxConstraints(
            BoxVariables const& container,
            std::vector<BoxVariables> const& children, Axis axis);

    /**
     * @brief Sizes and positions one content edge-pair within a slot edge-pair
     * on a single axis, reproducing gravity placement inside the solve.
     *
     * The content fills the slot — a weak pull equalising the two extents — but
     * never grows past @p maxExtent, a strong cap, so it settles at the smaller
     * of the slot and its own maximum. Where it is smaller than the slot it sits
     * at @p gravity of the slack: 0 against the leading edge, 1 against the
     * trailing, 0.5 centred. That placement is a weak pull, so a medium guide
     * alignment on the same edge overrides it. This is what
     * modifier::handleGravity() did as a post-pass, folded into the solve.
     *
     * @p gravity is the coefficient in the solver's top-down space; a caller
     * positioning on the vertical axis, which the widget tree flips to y-up,
     * passes 1 - g so a leading gravity still lands against the widget-space
     * leading edge.
     */
    BQUI_EXPORT void placeInSlot(std::vector<arrange::Constraint>& out,
            arrange::Variable const& contentLead,
            arrange::Variable const& contentTrail,
            arrange::Variable const& slotLead,
            arrange::Variable const& slotTrail,
            float gravity, float maxExtent);

    /**
     * @brief Lines each child edge named by a guide alignment up on a shared
     * per-guide line, pinning any guide an ancestor firewall already resolved.
     *
     * @p alignments is parallel to @p children: entry @e i lists the guides the
     * @e i-th child aligns to. Every alignment naming the same guide id, across
     * all children, is tied to one arrange::Variable minted for that id within
     * this call, so two children aligned to one guide meet on the edge they
     * name. The pull is medium strength — firmer than gravity, softer than a
     * size bound.
     *
     * A guide whose id is in @p resolved is pinned strong to that inherited
     * position, so it is a constant this solve lines its children up on rather
     * than a free variable it resolves itself — the constant half of the
     * firewall down-channel. A guide absent from @p resolved stays a free
     * coupling variable resolved locally as before.
     *
     * The per-guide line variables are appended to @p out and also returned
     * keyed by guide id, so a firewall can read a locally resolved line back out
     * of its solution and hand it to its descendants.
     */
    BQUI_EXPORT std::map<avg::UniqueId, arrange::Variable> guideConstraints(
            std::vector<arrange::Constraint>& out,
            std::vector<BoxVariables> const& children,
            std::vector<std::vector<GuideAlignment>> const& alignments,
            ResolvedGuideMap const& resolved);

    /**
     * @brief guideConstraints() with no inherited resolutions, returning just
     * the constraints. The single-firewall form.
     */
    BQUI_EXPORT std::vector<arrange::Constraint> guideConstraints(
            std::vector<BoxVariables> const& children,
            std::vector<std::vector<GuideAlignment>> const& alignments);

    /**
     * @brief One cell of a uniform grid: its lower-left corner (@p x, @p y) in
     * grid coordinates and its span (@p w columns by @p h rows).
     */
    struct GridCell
    {
        unsigned int x;
        unsigned int y;
        unsigned int w;
        unsigned int h;
    };

    /**
     * @brief The lines of a uniform grid, one variable per grid line on each
     * axis.
     *
     * @c xs holds the @e columns + 1 vertical lines left to right; @c ys the
     * @e rows + 1 horizontal lines indexed bottom to top, so @c ys[0] is the
     * container's bottom edge in the solver's top-down space and @c ys[rows] its
     * top. The box of the cell (@p x, @p y) spanning (@p w, @p h) is bounded by
     * @c xs[x]..xs[x+w] and @c ys[y]..ys[y+h].
     */
    struct GridLines
    {
        std::vector<arrange::Variable> xs;
        std::vector<arrange::Variable> ys;
    };

    /**
     * @brief Divides @p container into @p columns equal-width columns and
     * @p rows equal-height rows, appending the line constraints to @p out and
     * returning the lines.
     *
     * The grid lines span the container and are held to equal intervals, so
     * every column is the same width and every row the same height. Row 0 is the
     * bottom row: a cell at grid y grows upward from the container's bottom, so
     * once the solved boxes are flipped out of the solver's top-down space the
     * grid reads y-up from the origin. A caller places each child within its
     * cell box (bounded by the returned lines) with placeInSlot().
     */
    BQUI_EXPORT GridLines gridLines(std::vector<arrange::Constraint>& out,
            BoxVariables const& container, unsigned int columns,
            unsigned int rows);
} // namespace bqui::widget
