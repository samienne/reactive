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
     * @brief A widget's grow coefficient on one axis: its filler weight. A widget
     * contributes @c extent==coeff*F against its container's shared flex variable,
     * and the coefficient rides up in the band so a container holding a filler is
     * itself a filler to its parent.
     */
    struct Flex
    {
        float coeff;
    };

    /**
     * @brief The preferred (natural) extent of a box on one axis, with the
     * strength it is held at (weakest for a default, strong for a fixed size).
     * Distinct from the min/max bounds: the natural is the size the box settles
     * at inside them.
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
     * field, replacing whatever was there. The fields carry target extents
     * (values), not baked constraints, so a wrapper can grow a band by an inset
     * and re-tag it onto its outer box; a field is baked into a constraint at
     * flatten time, against the box it is emitted on.
     *
     * @c relations is everything else — tiling, wrapper inner/outer relations,
     * guide alignments, a filler's flex coupling — and is additive. It is a
     * LayoutSpec rather than a bare constraint list so the read-back variables
     * the solver API needs travel with the constraints that name them.
     */
    struct Constraints
    {
        std::optional<float> min;            ///< strong lower bound on extent
        std::optional<float> max;            ///< strong upper bound on extent
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
     * height band given the resolved width solution, and @ref getWidthForHeight
     * the width given the resolved height. Phase 2 receives the whole width
     * solution and each leaf reads its own resolved width from it. Phase 3
     * returns the phase-1 width unchanged.
     */
    struct PureLayout
    {
        /**
         * @brief The phase-2 shape: the height band given the resolved width
         * solution.
         */
        using WidthToConstraints = btl::Function<bq::signal::AnySignal<
            Constraints>(bq::signal::AnySignal<LayoutSolution>)>;

        bq::signal::AnySignal<Constraints> width;
        WidthToConstraints heightForWidth;

        /** @brief Phase 1: the width band and horizontal relations. */
        bq::signal::AnySignal<Constraints> getWidth() const
        {
            return width;
        }

        /**
         * @brief Phase 2: the height band given the resolved width solution
         * @p widthSolution, which each leaf reads its own resolved width from.
         */
        bq::signal::AnySignal<Constraints> getHeightForWidth(
                bq::signal::AnySignal<LayoutSolution> widthSolution) const
        {
            return heightForWidth(std::move(widthSolution));
        }

        /**
         * @brief Phase 3: the width given the resolved height, which is the
         * phase-1 width unchanged.
         */
        bq::signal::AnySignal<Constraints> getWidthForHeight(
                bq::signal::AnySignal<LayoutSolution> /*heightSolution*/) const
        {
            return width;
        }
    };
} // namespace bqui::widget
