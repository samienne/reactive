#pragma once

#include "curve/curves.h"
#include "curve.h"
#include "lerp.h"
#include "animationoptions.h"
#include "avgvisibility.h"

#include <btl/typetraits.h>

#include <optional>

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

    enum class RepeatMode
    {
        normal,
        reverse
    };

    template <typename T>
    class AVG_EXPORT_CLASS_TEMPLATE Animated
    {
        static_assert(IsEqualityComparable<T>::value);
        static_assert(HasLerp<T>::value);

    public:
        struct AVG_EXPORT KeyFrame
        {
            T target;
            Curve curve;
            std::chrono::milliseconds duration;
            RepeatMode repeat;
            int repeatCount;

            bool operator==(KeyFrame const& rhs) const
            {
                return target == rhs.target
                    && curve == rhs.curve
                    && duration == rhs.duration
                    && repeat == rhs.repeat
                    && repeatCount == rhs.repeatCount
                    ;
            }

            bool operator!=(KeyFrame const& rhs) const
            {
                return !(*this == rhs);
            }

            bool isInfinite() const
            {
                return repeatCount <= 0;
            }

            bool isActiveAt(std::chrono::milliseconds time) const
            {
                if (time < std::chrono::milliseconds(0))
                    return false;

                if (isInfinite())
                    return true;

                return time < (repeatCount * duration);
            }

            bool isReversedAt(std::chrono::milliseconds time) const
            {
                if (repeat != RepeatMode::reverse)
                    return false;

                if (duration <= std::chrono::milliseconds(0))
                    return false;

                return (time.count() / duration.count()) % 2 == 1;
            }
        };

        template <typename U, typename = std::enable_if_t<
            std::is_convertible_v<U, T>
            >>
        Animated(U&& value) :
            initial_(value),
            beginTime_(std::chrono::milliseconds(0))
        {
        }

        Animated(T initialValue, std::chrono::milliseconds beginTime,
                std::vector<KeyFrame> keyFrames) :
            initial_(std::move(initialValue)),
            beginTime_(beginTime),
            keyFrames_(std::move(keyFrames))
        {
        }

        Animated(T initialValue, T finalValue,
                Curve curve,
                std::chrono::milliseconds beginTime,
                std::chrono::milliseconds duration
                ) :
            initial_(std::move(initialValue)),
            beginTime_(beginTime),
            keyFrames_({ {
                    std::move(finalValue),
                    std::move(curve),
                    duration,
                    RepeatMode::normal,
                    1
                    }})
        {
        }

        T getValue(std::chrono::milliseconds time) const
        {
            if (keyFrames_.empty() || time <= beginTime_)
                return initial_;

            auto t = time - beginTime_;
            T const* value = &initial_;
            KeyFrame const* keyFrame;

            for (auto const& frame : keyFrames_)
            {
                keyFrame = &frame;
                if (keyFrame->isActiveAt(t))
                {
                    auto t2 = std::chrono::milliseconds(
                            t.count() % keyFrame->duration.count()
                            );

                    if (keyFrame->isReversedAt(t))
                        t2 = keyFrame->duration - t2;

                    t = t2;
                    break;
                }

                value = &frame.target;
                t -= frame.duration * frame.repeatCount;
            }

            float a = std::clamp(
                    (float)t.count() / (float)keyFrame->duration.count(),
                    0.0f,
                    1.0f
                    );

            return lerp(*value, keyFrame->target, keyFrame->curve(a));
        }

        std::vector<KeyFrame> const& getKeyFrames() const
        {
            return keyFrames_;
        }

        T const& getInitialValue() const
        {
            return initial_;
        }

        T const& getFinalValue() const
        {
            if (keyFrames_.empty())
                return initial_;

            return keyFrames_.back().target;
        }

        std::chrono::milliseconds getBeginTime() const
        {
            return beginTime_;
        }

        bool isInfinite() const
        {
            for (auto const& keyFrame : keyFrames_)
                if (keyFrame.isInfinite())
                    return true;

            return false;
        }

        std::optional<std::chrono::milliseconds> getDuration() const
        {
            if (isInfinite())
                return std::nullopt;

            if constexpr(HasLerp<T>::value)
            {
                std::chrono::milliseconds duration(0);
                for (auto const& keyFrame : keyFrames_)
                    duration += keyFrame.duration * keyFrame.repeatCount;

                return duration;
            }
            else
                return std::chrono::milliseconds(0);
        }

        bool hasAnimationEnded(std::chrono::milliseconds time) const
        {
            if (isInfinite())
                return false;

            return time >= (beginTime_ + *getDuration());
        }

        bool isAnimationRunning(std::chrono::milliseconds time) const
        {
            return beginTime_ < time && !hasAnimationEnded(time);
            //time < (beginTime_ + getDuration());
        }

        bool isRedundantUpdate(Animated const& newValue) const
        {
            // Expect additional keyframe added for transition
            if (getKeyFrames().size() != newValue.getKeyFrames().size() + 1
                    && getKeyFrames().size() != newValue.getKeyFrames().size())
            {
                return false;
            }

            // If there are no additional keyframes in the new value
            // the final values must be the same. Otherwise an update
            // is needed.
            if (newValue.getKeyFrames().empty())
                return getFinalValue() == newValue.getFinalValue();

            bool isRedundant = true;
            auto i = getKeyFrames().rbegin();
            auto j = newValue.getKeyFrames().rbegin();
            while (isRedundant
                    && i != getKeyFrames().rend()
                    && j != newValue.getKeyFrames().rend())
            {
                isRedundant = isRedundant && *i == *j;
                ++i;
                ++j;
            }

            return isRedundant;
        }

        Animated updated(
                Animated const& newValue,
                std::optional<AnimationOptions> const& options,
                std::chrono::milliseconds time
                ) const
        {
            if (isRedundantUpdate(newValue))
                return *this;

            if (!options)
            {
                return newValue;
            }

            std::vector<KeyFrame> keyFrames;
            keyFrames.reserve(1llu + newValue.getKeyFrames().size());

            keyFrames.push_back(KeyFrame{
                    newValue.getInitialValue(),
                    options->curve,
                    options->duration,
                    RepeatMode::normal,
                    1
                    });

            for (auto const& keyFrame : newValue.getKeyFrames())
                keyFrames.push_back(keyFrame);

            return Animated(
                    getValue(time),
                    time,
                    std::move(keyFrames)
                    );
        }

    private:
        T initial_;
        std::chrono::milliseconds beginTime_;
        std::vector<KeyFrame> keyFrames_;
    };

    class Rect;
    class Transform;
    class Obb;
    class Color;
    class Brush;
    class Pen;

    extern template class AVG_EXPORT_TEMPLATE Animated<float>;
    extern template class AVG_EXPORT_TEMPLATE Animated<Vector2f>;
    extern template class AVG_EXPORT_TEMPLATE Animated<Rect>;
    extern template class AVG_EXPORT_TEMPLATE Animated<Transform>;
    extern template class AVG_EXPORT_TEMPLATE Animated<Obb>;
    extern template class AVG_EXPORT_TEMPLATE Animated<Color>;
    extern template class AVG_EXPORT_TEMPLATE Animated<Brush>;
    extern template class AVG_EXPORT_TEMPLATE Animated<Pen>;
    extern template class AVG_EXPORT_TEMPLATE Animated<Curve>;

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

    template <typename T>
    auto infiniteAnimation(T from, T to, Curve curve, float seconds,
            RepeatMode repeat = RepeatMode::normal)
    {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::duration<float>(seconds)
                );

        return Animated<std::decay_t<T>>(std::move(from),
                std::chrono::milliseconds(0), {
                { std::move(to), std::move(curve), duration, repeat, 0 }
                });
    }

} // namespace avg

