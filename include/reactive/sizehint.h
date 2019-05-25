#pragma once

#include "reactivevisibility.h"

#include <ase/vector.h>

#include <btl/shared.h>
#include <btl/fmap.h>

#include <array>
#include <vector>

#include <type_traits>

namespace reactive
{
    enum class Axis
    {
        x,
        y
    };

    using SizeHintResult = std::array<float, 3>;

    template <typename T, typename = void>
    struct IsSizeHint : std::false_type {};

    template <typename T
    >
    struct IsSizeHint<T, std::enable_if_t<
            btl::All<
                std::is_same<SizeHintResult,
                    decltype(std::declval<T>().getWidth())
                >,
                std::is_same<SizeHintResult,
                    decltype(std::declval<T>().getHeight(100.0f))
                >,
                std::is_same<SizeHintResult,
                    decltype(std::declval<T>().getFinalWidth(100.0f, 100.0f))
                >
            >::value
        >
    > : std::true_type {};

    namespace detail
    {
        struct SizeHintBase
        {
            virtual ~SizeHintBase() = default;
            virtual SizeHintResult getWidth() const = 0;
            virtual SizeHintResult getHeight(float width) const = 0;
            virtual SizeHintResult getFinalWidth(float width,
                    float height) const = 0;
        };

        template <typename THint>
        struct SizeHintTyped final : SizeHintBase
        {
            SizeHintTyped(THint&& hint) :
                hint_(std::forward<THint>(hint))
            {
            }

            SizeHintResult getWidth() const override
            {
                return hint_.getWidth();
            }

            SizeHintResult getHeight(float width) const override
            {
                return hint_.getHeight(width);
            }

            SizeHintResult getFinalWidth(float width, float height) const override
            {
                return hint_.getFinalWidth(width, height);
            }

            std::decay_t<THint> const hint_;
        };
    } // detail


    /**
     * @brief Provides the preferred size for widget.
     *
     * SizeHint is a function that returns the size hints for the X-axis
     * and a function to retrieve the hints for the Y-axis. This is mainly
     * used by the layout system to determine how to allocate the window
     * real estate to specific widgets.
     *
     * The first part of the returned pair is an std::array<float, 3> where
     * the first element of the array is the minimum size for the widget,
     * the second part is the maximum size the widget may benefit from, and
     * the last part is the filler size.
     *
     * The requested sizes should be satisfied in increasing order. First the
     * minimum (first element), then maximum (the second element, and then the
     * filler (the third element).
     *
     * After determining the size for the X-axis the second part of the
     * returned pair can be used to retrieve the hints for the Y-axis by
     * giving the X-size to the function. The result of that function is used
     * the same way as the hints for the X-axis to determine the size for the
     * Y-axis.
     *
     * The simpleSizeHint function is the easiest way to create a size hint.
     */
    class REACTIVE_EXPORT SizeHint
    {
    public:
        SizeHint() = delete;

        template <typename THint, typename = std::enable_if_t<
            IsSizeHint<THint>::value
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

        SizeHintResult getWidth() const;
        SizeHintResult getHeight(float width) const;
        SizeHintResult getFinalWidth(float width, float height) const;

    private:
        btl::shared<detail::SizeHintBase> hint_;
    };

    REACTIVE_EXPORT SizeHintResult getLargestHint(
            std::vector<SizeHintResult> const& hints);

    REACTIVE_EXPORT std::ostream& operator<<(std::ostream& stream,
            SizeHintResult const& h);
}

