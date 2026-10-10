#pragma once

#include "bquivisibility.h"

#include <ase/vector.h>

#include <btl/shared.h>
#include <btl/fmap.h>

#include <optional>
#include <vector>

#include <type_traits>

namespace bqui
{
    enum class Axis
    {
        x,
        y
    };

    /**
     * @brief One axis's size band: the range a widget may take on that axis.
     *
     * @c min is the size below which the widget must not be squeezed, @c natural
     * the size it settles at when nothing pushes on it, and @c max the size it
     * can grow to. They are kept ordered min <= natural <= max.
     *
     * @c grow is the widget's filler weight: the share of a container's leftover
     * space it pulls in. Zero means the widget is not a filler and sits at its
     * natural size; a positive weight makes it a filler that grows past its
     * natural size, splitting the surplus with its siblings in proportion to the
     * weight (two grow=1 children split it equally, a grow=2 child takes twice
     * the surplus of a grow=1 one).
     */
    struct Band
    {
        float min = 0.0f;
        float natural = 0.0f;
        float max = 0.0f;
        float grow = 0.0f;
    };

    /**
     * @brief Placement anchors a widget exposes on one axis.
     *
     * A baseline is a metric measured from the axis origin (a function of the
     * widget's own size, never of the space a container allocates it). Both are
     * sparse: absent unless the widget defines them. A label reports its first
     * baseline on its vertical axis; a baseline-aligned row uses it to line its
     * children up (see baselineHbox).
     */
    struct Anchors
    {
        std::optional<float> firstBaseline;
        std::optional<float> lastBaseline;
    };

    /**
     * @brief A widget's hint for one axis: its size band and its anchors.
     */
    struct AxisHint
    {
        Band extent;
        Anchors anchors;
    };

    template <typename T, typename = void>
    struct IsSizeHint : std::false_type {};

    template <typename T
    >
    struct IsSizeHint<T, std::enable_if_t<
            btl::All<
                std::is_same<AxisHint,
                    decltype(std::declval<T>().getWidth())
                >,
                std::is_same<AxisHint,
                    decltype(std::declval<T>().getHeightForWidth(100.0f))
                >,
                std::is_same<AxisHint,
                    decltype(std::declval<T>().getWidthForHeight(100.0f))
                >
            >::value
        >
    > : std::true_type {};

    namespace detail
    {
        struct SizeHintBase
        {
            virtual ~SizeHintBase() = default;
            virtual AxisHint getWidth() const = 0;
            virtual AxisHint getHeightForWidth(float width) const = 0;
            virtual AxisHint getWidthForHeight(float height) const = 0;
        };

        template <typename THint>
        struct SizeHintTyped final : SizeHintBase
        {
            SizeHintTyped(THint&& hint) :
                hint_(std::forward<THint>(hint))
            {
            }

            AxisHint getWidth() const override
            {
                return hint_.getWidth();
            }

            AxisHint getHeightForWidth(float width) const override
            {
                return hint_.getHeightForWidth(width);
            }

            AxisHint getWidthForHeight(float height) const override
            {
                return hint_.getWidthForHeight(height);
            }

            std::decay_t<THint> const hint_;
        };
    } // detail


    /**
     * @brief Provides the preferred size for a widget.
     *
     * SizeHint answers three queries the layout system uses to allocate window
     * real estate: the width band, the height band for a chosen width, and the
     * width band for a chosen height. Each returns an AxisHint carrying that
     * axis's size Band ({min, natural, max, grow}) and its Anchors.
     *
     * Within a Band the layout satisfies the sizes in order: never below @c min,
     * settling at @c natural, and growing towards @c max only for a filler (one
     * with a positive @c grow). Fillers share a container's leftover space in
     * proportion to their @c grow weight.
     *
     * The two-step handshake sizes one axis and then the other: allocate a width
     * from getWidth(), feed it to getHeightForWidth() to size the height, or the
     * mirror through getWidthForHeight().
     *
     * The simpleSizeHint function is the easiest way to create a size hint.
     */
    class BQUI_EXPORT SizeHint
    {
    public:
        SizeHint() = delete;

        template <typename THint, typename = std::enable_if_t<
            IsSizeHint<THint>::value &&
            !std::is_same_v<SizeHint, std::decay_t<THint>>
            >>
        SizeHint(THint&& hint) :
            hint_(std::make_shared<
                    detail::SizeHintTyped<THint>
                    >(std::forward<THint>(hint)))
        {
        }

        SizeHint(SizeHint const& hint) = default;
        SizeHint(SizeHint&& hint) noexcept = default;

        SizeHint& operator=(SizeHint const&) = default;
        SizeHint& operator=(SizeHint&&) noexcept = default;

        template <typename THint, typename = std::enable_if_t<
            IsSizeHint<THint>::value
            >>
        SizeHint& operator=(THint&& hint)
        {
            hint_ = std::make_shared<detail::SizeHintTyped<THint>>(
                    std::forward<THint>(hint));

            return *this;
        }

        AxisHint getWidth() const;
        AxisHint getHeightForWidth(float width) const;
        AxisHint getWidthForHeight(float height) const;

    private:
        btl::shared<detail::SizeHintBase> hint_;
    };

    BQUI_EXPORT AxisHint getLargestHint(
            std::vector<AxisHint> const& hints);

    /**
     * @brief Aggregates a row of cross-axis hints split at their baselines.
     *
     * Each hint's band is divided at its firstBaseline into an ascent (the
     * offset to the baseline) and a descent (the rest), the two halves are
     * maxed across the row, and the results are recombined: the natural cross
     * size is @c maxAscent + @c maxDescent, and the aggregate reports its own
     * firstBaseline at @c maxAscent so an enclosing baseline row can align on
     * it in turn. A hint with no baseline contributes a zero ascent, so with
     * no baselines at all this reduces to getLargestHint and the aggregate
     * carries no baseline of its own.
     */
    BQUI_EXPORT AxisHint getBaselineHint(
            std::vector<AxisHint> const& hints);

    BQUI_EXPORT std::ostream& operator<<(std::ostream& stream,
            Band const& band);
}
