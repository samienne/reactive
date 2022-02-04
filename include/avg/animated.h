#pragma once

#include "curve/curves.h"
#include "curve.h"
#include "lerp.h"
#include "animationoptions.h"
#include "avgvisibility.h"

#include <btl/typetraits.h>

namespace avg
{
    template <typename T>
    using CompareType = decltype(
            std::declval<std::decay_t<T>>() == std::declval<std::decay_t<T>>()
            );

    template <typename T, typename = void>
    struct IsEqualityComparable : std::false_type {};

    template <typename T>
    struct IsEqualityComparable<T, btl::void_t<
        CompareType<T>
        >> : std::true_type {};

    template <typename T>
    class Animated
    {
        static_assert(IsEqualityComparable<T>::value);
        static_assert(HasLerp<T>::value);

    public:
        template <typename U, typename = std::enable_if_t<
            std::is_convertible_v<U, T>
            >>
        Animated(U&& value) :
            initial_(value),
            final_(value),
            curve_(curve::linear),
            beginTime_(std::chrono::milliseconds(0)),
            duration_(std::chrono::milliseconds(0))
        {
        }

        Animated(T initialValue, T finalValue,
                Curve curve,
                std::chrono::milliseconds beginTime,
                std::chrono::milliseconds duration
                ) :
            initial_(std::move(initialValue)),
            final_(std::move(finalValue)),
            curve_(std::move(curve)),
            beginTime_(beginTime),
            duration_(duration)
        {
        }

        T getValue(std::chrono::milliseconds time) const
        {
            if (getDuration() <= std::chrono::milliseconds(0))
            {
                return final_;
            }

            float a = std::clamp(
                    (float)(time - beginTime_).count() / (float)getDuration().count(),
                    0.0f,
                    1.0f
                    );

            if constexpr(HasLerp<T>::value)
                return lerp(initial_, final_, curve_(a));
            else
                return a <= 0.0f ? initial_ : final_;
        }

        T const& getInitialValue() const
        {
            return initial_;
        }

        T const& getFinalValue() const
        {
            return final_;
        }

        Curve const& getCurve() const
        {
            return curve_;
        };

        std::chrono::milliseconds getBeginTime() const
        {
            return beginTime_;
        }

        std::chrono::milliseconds getDuration() const
        {
            if constexpr(HasLerp<T>::value)
                return duration_;
            else
                return std::chrono::milliseconds(0);
        }

        bool hasAnimationEnded(std::chrono::milliseconds time) const
        {
            return time >= (beginTime_ + getDuration());
        }

        bool isAnimationRunning(std::chrono::milliseconds time) const
        {
            return beginTime_ < time && time < (beginTime_ + getDuration());
        }

        Animated updated(
                Animated const& newValue,
                std::optional<AnimationOptions> const& options,
                std::chrono::milliseconds time
                ) const
        {
            if (getFinalValue() == newValue.getFinalValue())
                return *this;

            if (!options)
            {
                if (isAnimationRunning(time))
                    return *this;
                else
                    return newValue;
            }

            return Animated(
                    getValue(time),
                    newValue.getFinalValue(),
                    options->curve,
                    time,
                    options->duration
                    );
        }

    private:
        T initial_;
        T final_;
        Curve curve_;
        std::chrono::milliseconds beginTime_;
        std::chrono::milliseconds duration_;
    };

    template <typename T>
    struct IsAnimated : std::false_type {};

    template <typename T>
    struct IsAnimated<Animated<T>> : std::true_type {};

    template <typename T>
    struct AnimatedType
    {
        using type = std::remove_reference_t<std::remove_cv_t<T>>;
    };

    template <typename T>
    struct AnimatedType<Animated<T>>
    {
        using type = std::remove_reference_t<std::remove_cv_t<T>>;
    };

    template <typename T>
    using AnimatedTypeT = typename AnimatedType<T>::type;

} // namespace avg

