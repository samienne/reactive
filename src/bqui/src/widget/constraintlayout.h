#pragma once

#include "bqui/bquivisibility.h"

#include "bqui/sizehint.h"
#include "bqui/widget/boxvariables.h"
#include "bqui/widget/guide.h"
#include "bqui/widget/layoutspec.h"
#include "bqui/widget/resolvedguides.h"

#include <bq/signal/constant.h>
#include <bq/signal/sharedvector.h>
#include <bq/signal/signal.h>

#include <avg/obb.h>

#include <arrange/constraint.h>
#include <arrange/expression.h>
#include <arrange/id.h>
#include <arrange/strength.h>
#include <arrange/variable.h>

#include <btl/function.h>

#include <cstddef>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace bqui::widget
{
    struct AnyBuilder;

    /**
     * @brief Appends one untagged relation fragment onto @p builder's descriptor
     * for @p axis (Axis::x horizontal, Axis::y vertical), minting the PureLayout
     * on the first fragment and composing onto it thereafter.
     *
     * Relations are additive: a filler's flex coupling, a guide alignment, a
     * container's tiling. The size band is set by name instead — see
     * setPureNatural() and friends — so a size modifier overrides by replacement
     * rather than piling a competing relation on.
     */
    void addPureConstraint(AnyBuilder& builder, Axis axis,
            bq::signal::AnySignal<LayoutSpec> fragment);

    /**
     * @brief Replaces the natural (preferred) extent band on @p axis with
     * @p value held at @p strength.
     *
     * The named-field write that makes a size word an override: a later
     * setPureNatural() on the same axis wins outright, with no competing
     * constraint left behind. A weakest strength is the leaf/container default; a
     * strong one is a fixed size.
     */
    void setPureNatural(AnyBuilder& builder, Axis axis,
            bq::signal::AnySignal<float> value, arrange::Strength strength);

    /**
     * @brief Replaces the lower-bound band on @p axis with @p value, held
     * strong so an unmeetable floor overflows rather than freezing the region.
     */
    void setPureMin(AnyBuilder& builder, Axis axis,
            bq::signal::AnySignal<float> value);

    /**
     * @brief Replaces the upper-bound band on @p axis with @p value, held
     * strong so a contradicting cap ties gracefully rather than freezing the
     * region.
     */
    void setPureMax(AnyBuilder& builder, Axis axis,
            bq::signal::AnySignal<float> value);

    /**
     * @brief Sets the pure band's @c min on @p axis to @p value only where it is
     * a genuine floor below the band's current natural, for bridging a leaf's
     * SizeHint lower bound. A bound equal to the natural is left off. Apply after
     * setPureNatural(); setPureMin() is the unconditional form.
     */
    void bridgePureMin(AnyBuilder& builder, Axis axis,
            bq::signal::AnySignal<float> value);

    /**
     * @brief Sets the pure band's @c max on @p axis to @p value only where it is
     * a genuine cap above the band's current natural. The ceiling counterpart of
     * bridgePureMin().
     */
    void bridgePureMax(AnyBuilder& builder, Axis axis,
            bq::signal::AnySignal<float> value);

    /**
     * @brief Replaces the flex (grow) coefficient band on @p axis.
     *
     * The summary a container reads to aggregate flex up; the coupling relation a
     * filler emits against its container's shared variable is a separate
     * addPureConstraint().
     */
    void setPureFlex(AnyBuilder& builder, Axis axis, float coeff);

    /**
     * @brief Synthesizes a PureLayout from @p hint on @p box, the universal
     * bridge for a builder that reaches a pure container without one.
     *
     * The width band comes from the hint's width and the height band from the
     * hint's height at @p box's resolved width, both held at content strength
     * with the min/max bounds bridged in where they widen the natural. A hint
     * that states no size preference (the framework default) bridges to an empty
     * band, so the container's weak default still sizes the box.
     */
    PureLayout pureLayoutFromSizeHint(bq::signal::AnySignal<SizeHint> hint,
            BoxVariables box);

    /**
     * @brief Wraps @p builder's descriptor in a fresh outer box inset by
     * @p inset on every edge, the solver half of an inset wrapper (margin,
     * padding, border).
     *
     * Mints an outer box, grows the named bands by the inset and re-tags them
     * onto it, appends the required inner/outer edge relations, and makes the
     * outer box the one the builder presents. Exactly one band lives on the
     * outermost box, so a later size word replaces it and the relation chain
     * distributes it inward through any depth of nesting.
     */
    void applyPureInset(AnyBuilder& builder, bq::signal::AnySignal<float> inset);

    /**
     * @brief Bakes one axis's @ref Constraints into a solver fragment on @p box.
     *
     * The band fields become constraints on the box's extent (@c natural at its
     * strength, @c min / @c max strong) and ride alongside the untagged
     * relations. @p axis selects the box's width or height as the extent.
     */
    LayoutSpec flattenConstraints(Constraints const& constraints,
            BoxVariables const& box, Axis axis);

    /**
     * @brief The up-channel a region owner threads down for its participating
     * containers to append their per-container spec fragments to.
     *
     * The presence of this entry is what marks a subtree as inside a banded
     * region: a container that finds it appends its own fragment and reads its
     * geometry from the shared LayoutSolutionTag rather than running its own
     * solve. The region owner (regionRoot) holds the sole owning reference to
     * the collector and folds its contents into the one region solve; this
     * down-channel carries only a non-owning handle. That is deliberate: the
     * collector owns the fragment signals, and a fragment reaches this params
     * entry through the child builders it is built from, so an owning handle
     * here would close a retain cycle collector -> fragment -> builder -> params
     * -> collector. The weak handle breaks it, exactly as the solution
     * down-channel's own back reference is weak. Fragments are appended as the
     * subtree builds, so the collection settles a pass behind the build, like
     * any change-driven input.
     */
    struct RegionCollectorTag
    {
        using type = std::weak_ptr<bq::signal::SharedVector<
            bq::signal::AnySignal<LayoutSpec>>>;

        static bq::signal::AnySignal<type> getDefaultValue()
        {
            return bq::signal::constant(type());
        }
    };

    /**
     * @brief The down-channel entry a widget in a banded firewall region reads
     * to learn the region's solved geometry.
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
     * @brief Whether the region a container builds into is a pure-solver context:
     * its members emit band-free constraints plus the universal weak defaults
     * rather than reading a SizeHint band.
     *
     * The default is false, so a region is the banded one unless a pure-solver
     * root seeds this true; the flag rides down alongside the region collector so
     * a container joining the region picks the matching fragment shape.
     */
    struct PureSolverTag
    {
        using type = bool;

        static bq::signal::AnySignal<bool> getDefaultValue()
        {
            return bq::signal::constant(false);
        }
    };

    /**
     * @brief The shared flex variable a pure-solver container mints for its
     * layout axis, seeded down for the fillers it holds to couple to.
     *
     * Every filler that is a direct child of the container adds
     * @c extent==F at the weakest tier against this one variable, so the
     * fillers all take the same extent and split the container's slack evenly;
     * the container's gap drive then pulls that shared extent up to absorb the
     * leftover. The value is a plain arrange variable (an id plus a name), not a
     * handle into the builder graph, so carrying it down forms no retain cycle.
     * Absent outside a pure-solver box, where a filler is inert.
     */
    struct FlexVariableTag
    {
        using type = arrange::Variable;

        static bq::signal::AnySignal<arrange::Variable> getDefaultValue()
        {
            return bq::signal::constant(arrange::Variable());
        }
    };

    /**
     * @brief The layout (main) axis of the pure-solver container a filler is a
     * direct child of, so the filler couples its extent on that axis.
     *
     * A filler constrains only the axis its container stacks along; the cross
     * axis falls to the container's own leading-edge pin and weak default. The
     * container seeds this alongside FlexVariableTag. Defaults to Axis::x, read
     * only when a filler actually sits inside a pure-solver box.
     */
    struct FlexAxisTag
    {
        using type = Axis;

        static bq::signal::AnySignal<Axis> getDefaultValue()
        {
            return bq::signal::constant(Axis::x);
        }
    };

    /**
     * @brief A box's stable solver identity and the per-axis constraints it
     * contributes to its context's one solve.
     *
     * The inverse of a SizeHint: a SizeHint is a size value aggregated up the
     * tree, this is a stable identity (the box's edge variables) plus a stream of
     * constraints fed down into a shared solve. The identity is a plain value and
     * only the constraints are signals, so a constraint change re-solves without
     * re-minting the box and the solver's id-keyed diff stays stable.
     *
     * The horizontal and vertical constraints are kept apart so the two axes
     * solve as two disjoint passes: pass 1 resolves the x-edges, pass 2 the
     * y-edges given the resolved width. The width handed to
     * getVerticalConstraints() is the box's own resolved width (right - left)
     * projected from pass 1, so a y-constraint may depend on the solved width
     * (height as a function of width) without a combined x+y solve.
     */
    class BoxDescriptor
    {
    public:
        using Constraints = bq::signal::AnySignal<std::vector<arrange::Constraint>>;

        BoxDescriptor(BoxVariables box, Constraints horizontal,
                btl::Function<Constraints(bq::signal::AnySignal<float>)> vertical) :
            box_(std::move(box)),
            horizontal_(std::move(horizontal)),
            vertical_(std::move(vertical))
        {
        }

        BoxVariables const& box() const
        {
            return box_;
        }

        Constraints getHorizontalConstraints() const
        {
            return horizontal_;
        }

        Constraints getVerticalConstraints(
                bq::signal::AnySignal<float> width) const
        {
            return vertical_(std::move(width));
        }

    private:
        BoxVariables box_;
        Constraints horizontal_;
        btl::Function<Constraints(bq::signal::AnySignal<float>)> vertical_;
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
     * @brief Unions a pure-solver region's two per-axis solutions into the one
     * solution a box reads its obb from.
     *
     * Pass 1 solves the x-edges and pass 2 the y-edges over disjoint variable
     * sets, so their id-keyed value maps never collide; this merges them into
     * the combined tableau readObb() reads left/right from the x-pass and
     * top/bottom from the y-pass out of.
     */
    BQUI_EXPORT bq::signal::AnySignal<LayoutSolution> combineSolutions(
            bq::signal::AnySignal<LayoutSolution> horizontal,
            bq::signal::AnySignal<LayoutSolution> vertical);

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
     * @brief The strictly-weakest strength tier, below gravity and natural size,
     * that the universal per-axis defaults sit at.
     *
     * Any real constraint dominates it, so a default only decides an axis nothing
     * else pinned. It is a distinct, far smaller weight than any other pull so it
     * never ties one and averages.
     */
    BQUI_EXPORT arrange::Strength weakestStrength();

    /**
     * @brief The strength a content leaf holds its content-measured natural at:
     * the top of the weak lane, above every other weak-lane pull yet a whole lane
     * below medium. Content sizes to its measurement by default; stretching (a
     * filler, a fixed size, a bound, a guide) is opt-in.
     */
    BQUI_EXPORT arrange::Strength contentStrength();

    /**
     * @brief The weak per-axis default @c width==100 on @p box.
     *
     * A leaf contributes this on each axis it does not otherwise size
     * (modifier::defaultSize()), and a stacking container adds it on its cross
     * axis; either way a box nothing else constrains resolves to a definite
     * width rather than leaving a free degree of freedom the solve is ill-posed
     * on. Add-only: minted with the box and never removed.
     */
    BQUI_EXPORT arrange::Constraint weakWidthDefault(BoxVariables const& box);

    /**
     * @brief The weak per-axis default @c height==100 on @p box, the vertical
     * counterpart of weakWidthDefault().
     */
    BQUI_EXPORT arrange::Constraint weakHeightDefault(BoxVariables const& box);

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
