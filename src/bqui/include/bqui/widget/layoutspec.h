#pragma once

#include <bq/signal/signal.h>

#include <arrange/constraint.h>
#include <arrange/id.h>
#include <arrange/strength.h>
#include <arrange/variable.h>

#include <btl/function.h>

#include <optional>
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
     * @brief A widget's grow coefficient: how much of a filler it is on one
     * axis.
     *
     * A filler contributes @c extent==coeff*F against its container's shared
     * flex variable; the coefficient rides up in the band so a container holding
     * a filler is itself a filler to its parent. The distribution stays the
     * container's solve — this is only the summary.
     */
    struct Flex
    {
        float coeff;
    };

    /**
     * @brief The preferred (natural) extent of a box on one axis, at the
     * strength it is held with.
     *
     * Distinct from the min/max bounds: the natural is the content/preferred
     * size the box settles at inside its bounds. Its strength is the "how firmly"
     * knob — weakest for a plain default, strong for a fixed size — so a fixed
     * size overrides a default by replacing this field rather than piling a
     * competing constraint on top.
     */
    struct BandNatural
    {
        float value;
        arrange::Strength strength;
    };

    /**
     * @brief One axis's size band plus the untagged relations that ride with it.
     *
     * The band is the four named, overridable-by-replacement fields
     * (@c min / @c max / @c natural / @c flex): a size modifier sets its one
     * field, replacing whatever was there, so there is no strength ladder to
     * exhaust and no over-constraint. The named fields carry target extents
     * (values), not pre-baked solver constraints, precisely so a wrapper can grow
     * a band by an inset and re-tag it onto its outer box; a constraint is baked
     * from a field only at flatten time, against the box it is emitted on.
     *
     * @c relations is everything else — tiling, wrapper inner/outer relations,
     * guide alignments, a filler's flex coupling — and is additive: relations
     * accumulate. It is a LayoutSpec rather than a bare constraint list so the
     * read-back variables the solver API needs travel with the constraints that
     * name them.
     */
    struct Constraints
    {
        std::optional<float> min;            ///< required lower bound on extent
        std::optional<float> max;            ///< required upper bound on extent
        std::optional<BandNatural> natural;  ///< preferred extent, at a strength
        std::optional<Flex> flex;            ///< filler coefficient (aggregated)
        LayoutSpec relations;                ///< untagged relations + read-backs
    };

    /**
     * @brief A subtree's pure-solver descriptor: the banded constraints its
     * current outermost box contributes to its region's solve, per phase.
     *
     * The inverse of a SizeHint. Where a SizeHint is a size value aggregated up
     * the tree, this is a stable identity (the box's edge variables, carried on
     * the builder) plus a stream of banded constraints a firewall reads off the
     * builder and solves. Absent outside a pure-solver region.
     *
     * The three phase functions each return one axis's @ref Constraints for the
     * outermost box: @ref getWidth resolves the width, @ref getHeightForWidth the
     * height given the resolved width, and @ref getWidthForHeight the width given
     * the resolved height. Phase 3 is a no-op returning the phase-1 width for
     * every widget here; phase 2 is width-independent for the widgets this
     * carries (content-sizing, which makes a height depend on the width, is a
     * later increment), so both simply ignore their argument for now while the
     * signatures keep the staging point open.
     */
    struct PureLayout
    {
        /**
         * @brief The phase-2 shape: the height band given the resolved width.
         */
        using WidthToConstraints = btl::Function<
            bq::signal::AnySignal<Constraints>(bq::signal::AnySignal<float>)>;

        bq::signal::AnySignal<Constraints> width;
        WidthToConstraints heightForWidth;

        /** @brief Phase 1: the width band and horizontal relations. */
        bq::signal::AnySignal<Constraints> getWidth() const
        {
            return width;
        }

        /** @brief Phase 2: the height band given the resolved width @p w. */
        bq::signal::AnySignal<Constraints> getHeightForWidth(
                bq::signal::AnySignal<float> w) const
        {
            return heightForWidth(std::move(w));
        }

        /** @brief Phase 3 (stub): the width given the resolved height, which for
         * every widget here is the phase-1 width unchanged. */
        bq::signal::AnySignal<Constraints> getWidthForHeight(
                bq::signal::AnySignal<float> /*h*/) const
        {
            return width;
        }
    };
} // namespace bqui::widget
