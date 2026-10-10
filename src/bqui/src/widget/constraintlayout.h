#pragma once

#include "bqui/bquivisibility.h"

#include "bqui/widget/boxvariables.h"
#include "bqui/widget/layoutspec.h"

#include <bq/signal/constant.h>
#include <bq/signal/signal.h>

#include <avg/obb.h>

#include <arrange/constraint.h>
#include <arrange/expression.h>
#include <arrange/id.h>
#include <arrange/strength.h>
#include <arrange/variable.h>

#include <btl/function.h>

#include <vector>

namespace bqui
{
    enum class Axis
    {
        x,
        y
    };
}

namespace bqui::widget
{
    struct AnyBuilder;

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
     * @brief Fixes the extent on @p axis at @p value: a strong natural that also
     * clears the band's flex there.
     *
     * A fixed size replaces a flex set before it (a fill() or one aggregated up
     * from a flexing child), so the widget's container stamps the natural and
     * couples nothing. A later setPureFlex() re-enables the flex, the natural
     * then riding as its flex-basis. The bounds are left alone.
     */
    void setPureFixed(AnyBuilder& builder, Axis axis,
            bq::signal::AnySignal<float> value);

    /**
     * @brief Replaces the flex band on @p axis with @p weight, making the widget
     * flexible there.
     *
     * The band's natural stays as a flex-basis, which the container drops when it
     * stamps the widget; the bounds still clamp the flexed extent.
     */
    void setPureFlex(AnyBuilder& builder, Axis axis,
            bq::signal::AnySignal<float> weight);

    /**
     * @brief Replaces the lower-bound band on @p axis with @p value, held at
     * minStrength() so an unmeetable floor overflows rather than freezing the
     * region.
     */
    void setPureMin(AnyBuilder& builder, Axis axis,
            bq::signal::AnySignal<float> value);

    /**
     * @brief Replaces the upper-bound band on @p axis with @p value, held at
     * maxStrength(): it caps a fixed size but yields to a contradicting min.
     */
    void setPureMax(AnyBuilder& builder, Axis axis,
            bq::signal::AnySignal<float> value);

    /**
     * @brief Publishes the anchor @p id on @p axis, replacing one with the
     * same id.
     */
    void setPureAnchor(AnyBuilder& builder, Axis axis, AnchorId id,
            bq::signal::AnySignal<Anchor> anchor);

    /**
     * @brief Binds the point @p at on @p axis to @p guide, alongside any
     * bindings already on the band.
     */
    void setPureGuide(AnyBuilder& builder, Axis axis, arrange::Variable guide,
            Anchor at);

    /**
     * @brief Binds the anchor @p id on @p axis to @p guide, as it stands
     * when this is applied; a band without that anchor gains no binding.
     */
    void setPureGuideAnchor(AnyBuilder& builder, Axis axis,
            arrange::Variable guide, AnchorId id);

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
     * strength, @c min and a floor at zero at minStrength(), @c max at
     * maxStrength()) and ride
     * alongside the untagged relations. @p axis selects the box's width or
     * height as the extent. Guide bindings and gravity placements resolve
     * against the box's @p gravity.
     */
    LayoutSpec flattenConstraints(Constraints const& constraints,
            BoxVariables const& box, Axis axis, avg::Vector2f gravity);

    /**
     * @brief The leading fraction of @p gravity on @p axis: left to right on
     * x, top down on y, where gravity's y runs bottom up.
     */
    float leadingGravity(avg::Vector2f gravity, Axis axis);

    /**
     * @brief @p anchor with its gravity part resolved at the leading
     * fraction @p gravity.
     */
    Anchor resolveAnchor(BandAnchor const& anchor, float gravity);

    /**
     * @brief The layout (main) axis of the pure-solver container a filler is a
     * direct child of, so the filler flexes on that axis; empty for a container
     * with no layout axis (a grid), where it flexes on both.
     *
     * A filler publishes its flex only on the axis its container stacks along,
     * where the container couples it to its slack; the cross axis falls to the
     * container's own leading-edge pin and weak default. In a grid it fills its
     * cell on both axes. Defaults to Axis::x, read only when a filler actually
     * sits inside a pure-solver container.
     */
    struct FlexAxisTag
    {
        using type = std::optional<Axis>;

        static bq::signal::AnySignal<std::optional<Axis>> getDefaultValue()
        {
            return bq::signal::constant(std::optional<Axis>(Axis::x));
        }
    };

    /**
     * @brief A box's stable solver identity and the per-axis constraints it
     * contributes to its context's one solve.
     *
     * It is a stable identity (the box's edge variables) plus a stream of
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
     * region's spec fragments into one tableau and solves them together,
     * yielding the shared solution every participating box reads its obb from.
     *
     * The fragments are every container's own constraints in the one shared
     * coordinate space, so constraints couple across container levels. The
     * solution is handed down to the region's builds as a build argument.
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
     * @brief Pins a box to a fixed window-space rectangle, held at
     * fixedStrength().
     *
     * Not required, so a box that is both anchored and tiled by a parent
     * overflows against the tiling instead of making the region solve
     * infeasible, and below the region's own anchor; unopposed, the anchor
     * resolves the box to the exact rectangle.
     */
    BQUI_EXPORT std::vector<arrange::Constraint> anchorConstraints(
            BoxVariables const& box,
            float left, float top, float right, float bottom);

    /**
     * @brief The strength a region holds its outermost box at the assigned
     * size: above every stated size so the region always takes its size, yet
     * not required, so nothing a widget states can make the solve infeasible.
     */
    BQUI_EXPORT arrange::Strength regionAnchorStrength();

    /**
     * @brief The strength of a @c min bound and of the @c >=0 extent floor,
     * the firmest stated size: a minimum beats a contradicting maximum and a
     * fixed size.
     */
    BQUI_EXPORT arrange::Strength minStrength();

    /**
     * @brief The strength of a @c max bound: below a minimum, above a fixed
     * size.
     */
    BQUI_EXPORT arrange::Strength maxStrength();

    /**
     * @brief The strength of a fixed size, the weakest strong pull: a bound it
     * contradicts wins and the fixed size yields.
     */
    BQUI_EXPORT arrange::Strength fixedStrength();

    /**
     * @brief The strength a row aligns its children's anchors at: above a
     * fixed size, below the bounds.
     *
     * The shared line and each aligned child's cross position are otherwise
     * free, so an alignment only yields when something else pins the child.
     */
    BQUI_EXPORT arrange::Strength alignStrength();

    /**
     * @brief The strength a point is pulled onto its guide at: the weakest
     * strong pull, so any stated size beats it and it beats every content
     * natural and placement.
     */
    BQUI_EXPORT arrange::Strength guideStrength();

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
     * filler, a fixed size, a bound) is opt-in.
     */
    BQUI_EXPORT arrange::Strength contentStrength();

    /**
     * @brief The weak per-axis default @c width==100 on @p box.
     *
     * A leaf contributes this on each axis it does not otherwise size
     * (modifier::defaultSize(size)), and a stacking container adds it on its cross
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
     * @brief One axis's filler band: a unit flex when @p thisAxis is the
     * container's @p layoutAxis or the container has none, an empty band
     * otherwise.
     *
     * So a filler fills the layout axis by flex, which its container couples to
     * its slack, and a cross axis by the container's cross-fill.
     */
    Constraints fillerAxisBand(Axis thisAxis, std::optional<Axis> layoutAxis);

    /**
     * @brief Sizes and positions one content edge-pair within a slot edge-pair
     * on a single axis, reproducing gravity placement inside the solve.
     *
     * The content fills the slot — a weak pull equalising the two extents — but
     * never grows past @p maxExtent, a cap at maxStrength(), so it settles at the smaller
     * of the slot and its own maximum. Where it is smaller than the slot it sits
     * at @p gravity of the slack: 0 against the leading edge, 1 against the
     * trailing, 0.5 centred. That placement is a weak pull, so a medium
     * constraint on the same edge overrides it. This is what
     * modifier::handleGravity() did as a post-pass, folded into the solve.
     *
     * @p gravity is the coefficient in the solver's top-down space; a caller
     * positioning on the vertical axis, which the widget tree flips to y-up,
     * passes 1 - g so a leading gravity still lands against the widget-space
     * leading edge.
     *
     * @p fillStrength weights the fill equality. The default suits a slot the
     * caller has already sized; a caller whose slot is free to follow its
     * content (an overlay container sizing to a filler) passes a strength below
     * the slack drive so the fill lifts the content to the slot without dragging
     * the slot down to a fixed-size child.
     */
    BQUI_EXPORT void placeInSlot(std::vector<arrange::Constraint>& out,
            arrange::Variable const& contentLead,
            arrange::Variable const& contentTrail,
            arrange::Variable const& slotLead,
            arrange::Variable const& slotTrail,
            float gravity, float maxExtent,
            arrange::Strength fillStrength = arrange::Strength::weak());

    /**
     * @brief Positions content of its own extent at the @p gravity fraction of
     * the slack within a slot, without pulling its extent to the slot.
     *
     * The gravity half of placeInSlot(), for content that holds its own size: a
     * fill it resisted would drag a slot that is free to follow its content.
     */
    BQUI_EXPORT void placeAtGravity(std::vector<arrange::Constraint>& out,
            arrange::Variable const& contentLead,
            arrange::Variable const& contentTrail,
            arrange::Variable const& slotLead,
            arrange::Variable const& slotTrail,
            float gravity);

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
     * @brief Pins one pre-minted family of grid lines to equal intervals
     * spanning @p spanLead to @p spanTrail, appending the constraints to @p out.
     *
     * The front line is pinned to @p spanLead and the back to @p spanTrail, and
     * every interior interval is held equal to its neighbour, so the lines
     * divide the span evenly. The line variables are the caller's. gridLines()
     * is the both-axes convenience over this.
     */
    BQUI_EXPORT void gridAxisConstraints(std::vector<arrange::Constraint>& out,
            std::vector<arrange::Variable> const& lines,
            arrange::Variable const& spanLead,
            arrange::Variable const& spanTrail);

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
