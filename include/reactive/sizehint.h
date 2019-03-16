#pragma once

#include <ase/vector.h>

#include <btl/shared.h>
#include <btl/fmap.h>

#include <array>
#include <functional>
#include <vector>

#include <type_traits>
#include <ostream>
#include <cassert>

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

    class SizeHint
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
        SizeHint(SizeHint&& hint) = default;

        SizeHint& operator=(SizeHint const&) = default;
        SizeHint& operator=(SizeHint&&) = default;

        template <typename THint, typename = std::enable_if_t<
            IsSizeHint<THint>::value
            >>
        SizeHint& operator=(THint&& hint)
        {
            hint_ = std::make_shared<detail::SizeHintTyped<THint>>(
                    std::forward<THint>(hint));

            return *this;
        }

        SizeHintResult operator()() const
        {
            return (*hint_)();
        }

        SizeHintResult operator()(float x) const
        {
            return (*hint_)(x);
        }

        SizeHintResult operator()(float x, float y) const
        {
            return (*hint_)(x, y);
        }

    private:
        btl::shared<detail::SizeHintBase> hint_;
    };

    static_assert(IsSizeHint<SizeHint>::value, "");

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
    //using SizeHint = std::function<SizeHintPartial()>;

    class SimpleSizeHint
    {
    public:
        SimpleSizeHint(SizeHintResult x, SizeHintResult y) :
            horizontal_(x),
            vertical_(y)
        {
        }

        SizeHintResult operator()() const
        {
            return horizontal_;
        }

        SizeHintResult operator()(float) const
        {
            return vertical_;
        }

        SizeHintResult operator()(float, float) const
        {
            return horizontal_;
        }

    private:
        SizeHintResult const horizontal_;
        SizeHintResult const vertical_;
    };

    static_assert(IsSizeHint<SimpleSizeHint>::value, "");

    /**
     * @brief Creates static size hint.
     *
     * This is the simplest way to create a size hint. The returned sizes
     * are determined by the given parameters.
     *
     * @param x The hints on the X-axis.
     * @param y The hints on the Y-axis.
     * @return SizeHint that will return the x and y hints.
     */
    inline auto simpleSizeHint(SizeHintResult x, SizeHintResult y)
        -> SimpleSizeHint
    {
        return SimpleSizeHint{std::move(x), std::move(y)};
    }

    /**
     * @brief Equivalent of simpleSizeHint({{x, x, x}}, {{y, y, y}}).
     */
    inline auto simpleSizeHint(float x, float y) -> SimpleSizeHint
    {
        return simpleSizeHint({{x, x, x}}, {{y, y, y}});
    }

    inline auto combinePartialHints(std::vector<SizeHintResult> const& hints)
        -> SizeHintResult
    {
        auto result = SizeHintResult{{0.0f, 0.0f, 0.0f}};
        for (auto const& hint : hints)
            for (int i = 0; i < 3; ++i)
                result[i] += hint[i];

        return result;
    }

    inline auto getLargestHint(std::vector<SizeHintResult> const& hints)
        -> SizeHintResult
    {
        auto result = SizeHintResult{{0.0f, 0.0f, 0.0f}};
        for (auto const& hint : hints)
            for (int i = 0; i < 3; ++i)
                result[i] = std::max(result[i], hint[i]);

        return result;
    }

    inline auto getSizes(float size,
            std::vector<std::array<float, 3>> const& hints)
        -> std::vector<float>
    {
        std::vector<float> result;
        result.reserve(hints.size());

        auto combined = combinePartialHints(hints);
        std::array<float, 3> multiplier;
        for (size_t i = 0; i < multiplier.size(); ++i)
        {
            float prev = (i ? combined[i-1] : 0.0f);
            float m = (size - prev) / (combined[i] - prev);
            multiplier[i] = std::max(0.0f, std::min(1.0f, m));
        }

        for (auto const& hint : hints)
        {
            float r = 0.0f;
            for (size_t i = 0; i < hint.size(); ++i)
            {
                float prev = (i ? hint[i-1] : 0.0f);
                r += (hint[i] - prev) * multiplier[i];
            }
            result.push_back(r);
        }

        return result;
    }

    inline SizeHintResult growSizeHintResult(SizeHintResult const& result,
            float amount)
    {
        return SizeHintResult{
            {
                result[0] + amount * 2.0f,
                result[1] + amount * 2.0f,
                result[2] + amount * 2.0f
            }
        };
    }

    inline std::ostream& operator<<(std::ostream& stream,
            SizeHintResult const& h)
    {
        stream << "SizeHintResult{"
            << "h:{";

        for (int i = 0; i < 3; ++i)
            stream << h[i] << (i < 2 ? "," :"");

        return stream << "}";
    }
}

