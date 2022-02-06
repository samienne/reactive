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
        struct KeyFrame
        {
            T target;
            Curve curve;
            std::chrono::milliseconds duration;

            bool operator==(KeyFrame const& rhs) const
            {
                return target == rhs.target
                    && curve == rhs.curve
                    && duration == rhs.duration;
            }

            bool operator!=(KeyFrame const& rhs) const
            {
                return !(*this == rhs);
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
                    duration
                    }})
        {
        }

        T getValue(std::chrono::milliseconds time) const
        {
            if (keyFrames_.empty())
                return initial_;

            auto t = time - beginTime_;
            T const* value = &initial_;
            KeyFrame const* keyFrame;

            for (auto const& frame : keyFrames_)
            {
                keyFrame = &frame;
                if (t < keyFrame->duration)
                    break;

                value = &frame.target;
                t -= frame.duration;
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

        KeyFrame const& getKeyFrameAt(std::chrono::milliseconds time) const
        {
            assert(!keyFrames_.empty());

            auto t = time - beginTime_;
            for (auto const& keyFrame : keyFrames_)
            {
                if (t < std::chrono::milliseconds(0))
                    return keyFrame;

                t -= keyFrame.duration;
            }

            return keyFrames_.back();
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

        std::chrono::milliseconds getDuration() const
        {
            if constexpr(HasLerp<T>::value)
            {
                std::chrono::milliseconds duration(0);
                for (auto const& keyFrame : keyFrames_)
                    duration += keyFrame.duration;

                return duration;
            }
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

        bool isRedundantUpdate(Animated const& newValue) const
        {
            // Expect additional keyframe added for transition
            if (getKeyFrames().size() != newValue.getKeyFrames().size() + 1)
                return false;

            // If there are no additional keyframes in the new value
            // the final values must be the same. Otherwise an update
            // is needed.
            if (newValue.getKeyFrames().empty())
                return getFinalValue() == newValue.getFinalValue();

            bool isRedundant = true;
            auto i = ++getKeyFrames().begin();
            auto j = newValue.getKeyFrames().begin();
            while (isRedundant
                    && i != getKeyFrames().end()
                    && j != newValue.getKeyFrames().end())
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
                if (isAnimationRunning(time))
                    return *this;
                else
                    return newValue;
            }

            if (newValue.getKeyFrames().empty())
            {
                return Animated(
                        getValue(time),
                        newValue.getFinalValue(),
                        options->curve,
                        time,
                        options->duration
                        );
            }
            else
            {
                std::vector<KeyFrame> keyFrames;
                keyFrames.reserve(1llu + newValue.getKeyFrames().size());

                keyFrames.push_back(KeyFrame{
                        newValue.getInitialValue(),
                        options->curve,
                        options->duration
                        });

                for (auto const& keyFrame : newValue.getKeyFrames())
                    keyFrames.push_back(keyFrame);

                return Animated(
                        getValue(time),
                        time,
                        std::move(keyFrames)
                        );

            }
        }

    private:
        T initial_;
        std::chrono::milliseconds beginTime_;
        std::vector<KeyFrame> keyFrames_;
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

