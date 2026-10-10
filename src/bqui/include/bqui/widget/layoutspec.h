#pragma once

#include "bqui/bquivisibility.h"

#include <bq/signal/signal.h>

#include <arrange/constraint.h>
#include <arrange/id.h>
#include <arrange/strength.h>
#include <arrange/variable.h>

#include <btl/function.h>
#include <btl/fmap.h>
#include <btl/shared.h>

#include <memory>
#include <optional>
#include <type_traits>
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
     * @brief The phase-2/phase-3 shape: a widget's height (or width) band given a
     * resolved perpendicular-axis solution, from which each leaf reads its own
     * box.
     */
    using WidthToConstraints = btl::Function<bq::signal::AnySignal<
        Constraints>(bq::signal::AnySignal<LayoutSolution>)>;

    template <typename T, typename = void>
    struct IsPureLayout : std::false_type {};

    template <typename T>
    struct IsPureLayout<T, std::enable_if_t<
            btl::All<
                std::is_same<bq::signal::AnySignal<Constraints>,
                    decltype(std::declval<T>().getWidth())
                >,
                std::is_same<bq::signal::AnySignal<Constraints>,
                    decltype(std::declval<T>().getHeightForWidth(
                        std::declval<bq::signal::AnySignal<LayoutSolution>>()))
                >,
                std::is_same<bq::signal::AnySignal<Constraints>,
                    decltype(std::declval<T>().getWidthForHeight(
                        std::declval<bq::signal::AnySignal<LayoutSolution>>()))
                >
            >::value
        >
    > : std::true_type {};

    namespace detail
    {
        struct PureLayoutBase
        {
            virtual ~PureLayoutBase() = default;
            virtual bq::signal::AnySignal<Constraints> getWidth() const = 0;
            virtual bq::signal::AnySignal<Constraints> getHeightForWidth(
                    bq::signal::AnySignal<LayoutSolution> widthSolution)
                const = 0;
            virtual bq::signal::AnySignal<Constraints> getWidthForHeight(
                    bq::signal::AnySignal<LayoutSolution> heightSolution)
                const = 0;
        };

        template <typename T>
        struct PureLayoutTyped final : PureLayoutBase
        {
            PureLayoutTyped(T&& impl) :
                impl_(std::forward<T>(impl))
            {
            }

            bq::signal::AnySignal<Constraints> getWidth() const override
            {
                return impl_.getWidth();
            }

            bq::signal::AnySignal<Constraints> getHeightForWidth(
                    bq::signal::AnySignal<LayoutSolution> widthSolution)
                const override
            {
                return impl_.getHeightForWidth(std::move(widthSolution));
            }

            bq::signal::AnySignal<Constraints> getWidthForHeight(
                    bq::signal::AnySignal<LayoutSolution> heightSolution)
                const override
            {
                return impl_.getWidthForHeight(std::move(heightSolution));
            }

            std::decay_t<T> const impl_;
        };
    } // detail

    /**
     * @brief A subtree's pure-solver descriptor: the banded constraints its
     * current outermost box contributes to its region's solve, per phase.
     *
     * The inverse of a SizeHint. Where a SizeHint is a size value aggregated up
     * the tree, this is a stable identity (the box's edge variables, carried on
     * the builder) plus a stream of banded constraints a firewall reads off the
     * builder and solves. Absent outside a pure-solver region.
     *
     * A type-erased holder over any type implementing the three phase functions,
     * so a widget can supply its own (a genuine width-for-height for aspect-locked
     * content, say). @ref getWidth resolves the width, @ref getHeightForWidth the
     * height band given the resolved width solution, and @ref getWidthForHeight
     * the width given the resolved height. Phase 2 and phase 3 receive the whole
     * perpendicular solution, from which each leaf reads its own box.
     *
     * The simplePureLayout function is the easiest way to build one.
     */
    class BQUI_EXPORT PureLayout
    {
    public:
        PureLayout() = delete;

        template <typename T, typename = std::enable_if_t<
            IsPureLayout<T>::value &&
            !std::is_same_v<PureLayout, std::decay_t<T>>
            >>
        PureLayout(T&& impl) :
            impl_(std::make_shared<detail::PureLayoutTyped<T>>(
                    std::forward<T>(impl)))
        {
        }

        PureLayout(PureLayout const&) = default;
        PureLayout(PureLayout&&) noexcept = default;

        PureLayout& operator=(PureLayout const&) = default;
        PureLayout& operator=(PureLayout&&) noexcept = default;

        bq::signal::AnySignal<Constraints> getWidth() const;
        bq::signal::AnySignal<Constraints> getHeightForWidth(
                bq::signal::AnySignal<LayoutSolution> widthSolution) const;
        bq::signal::AnySignal<Constraints> getWidthForHeight(
                bq::signal::AnySignal<LayoutSolution> heightSolution) const;

    private:
        btl::shared<detail::PureLayoutBase> impl_;
    };

    /**
     * @brief The default PureLayout implementation: a fixed width band and a
     * height-for-width function, with phase 3 the phase-1 width unchanged.
     */
    struct SimplePureLayout
    {
        bq::signal::AnySignal<Constraints> width;
        WidthToConstraints heightForWidth;

        bq::signal::AnySignal<Constraints> getWidth() const
        {
            return width;
        }

        bq::signal::AnySignal<Constraints> getHeightForWidth(
                bq::signal::AnySignal<LayoutSolution> widthSolution) const
        {
            return heightForWidth(std::move(widthSolution));
        }

        bq::signal::AnySignal<Constraints> getWidthForHeight(
                bq::signal::AnySignal<LayoutSolution> /*heightSolution*/) const
        {
            return width;
        }
    };

    /**
     * @brief Builds the default PureLayout from a width band and a
     * height-for-width function.
     */
    BQUI_EXPORT PureLayout simplePureLayout(
            bq::signal::AnySignal<Constraints> width,
            WidthToConstraints heightForWidth);
} // namespace bqui::widget
