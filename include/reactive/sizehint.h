#pragma once

#include <ase/vector.h>

#include <btl/shared.h>
#include <btl/fmap.h>
#include <btl/visibility.h>

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
    struct IsSizeHint<T,
             std::enable_if_t<
            std::is_same<SizeHintResult,
                std::result_of_t<T const()>
                >::value
            && std::is_same<SizeHintResult,
                std::result_of_t<T const(float)>
                >::value
            && std::is_same<SizeHintResult,
                std::result_of_t<T const(float,float)>
                >::value
        >
    > : std::true_type {};

    namespace detail
    {
        struct SizeHintBase
        {
            virtual ~SizeHintBase() = default;
            virtual SizeHintResult operator()() const = 0;
            virtual SizeHintResult operator()(float x) const = 0;
            virtual SizeHintResult operator()(float x, float y) const = 0;
        };

        template <typename THint>
        struct SizeHintTyped final : SizeHintBase
        {
            SizeHintTyped(THint&& hint) :
                hint_(std::forward<THint>(hint))
            {
            }

            SizeHintResult operator()() const override
            {
                return hint_();
            }

            SizeHintResult operator()(float x) const override
            {
                return hint_(x);
            }

            SizeHintResult operator()(float x, float y) const override
            {
                return hint_(x, y);
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
    class BTL_VISIBLE SizeHint
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

        SizeHintResult operator()() const;
        SizeHintResult operator()(float x) const;
        SizeHintResult operator()(float x, float y) const;

    private:
        btl::shared<detail::SizeHintBase> hint_;
    };

    BTL_VISIBLE SizeHintResult getLargestHint(
            std::vector<SizeHintResult> const& hints);

    BTL_VISIBLE std::ostream& operator<<(std::ostream& stream,
            SizeHintResult const& h);
}

