#pragma once

#include "setparams.h"
#include "provideanimation.h"

#include <reactive/signal2/signal.h>

#include <avg/curve.h>
#include <avg/animationoptions.h>

namespace reactive::widget
{
    template <typename T>
    auto setAnimation(
            signal2::Signal<T, std::optional<avg::AnimationOptions>> animationOptions)
    {
        return setParams<AnimationTag>(std::move(animationOptions).share());
    }

    inline auto setAnimation(avg::AnimationOptions options)
    {
        return setAnimation(
                signal2::constant<std::optional<avg::AnimationOptions>>(
                    std::move(options)
                    )
                );
    }

    template <typename T, typename U>
    auto setAnimation(avg::AnimationOptions options, signal2::Signal<T, U> signal)
    {
        return makeWidgetModifier(
                [](auto widget, auto animation, auto options, auto signal)
                {
                    return std::move(widget)
                        | setAnimation(
                            merge(signal.changed(), std::move(animation))
                            .map([options=std::move(options)]
                                (bool hasChanged,
                                 std::optional<avg::AnimationOptions> animation)
                                -> std::optional<avg::AnimationOptions>
                                {
                                    return hasChanged
                                        ? std::make_optional(options)
                                        : animation
                                        ;
                                })
                            );
                },
                provideAnimation(),
                std::move(options),
                std::move(signal)
                );
    }

    inline auto setAnimation(std::nullopt_t)
    {
        return setAnimation(
                signal2::constant<std::optional<avg::AnimationOptions>>(
                    std::nullopt
                    )
                );
    }

    inline auto setAnimation(float duration, avg::Curve curve)
    {
        return setAnimation(avg::AnimationOptions{
                std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::duration<float>(duration)
                        ),
                std::move(curve)
                });
    }

    template <typename T, typename U>
    auto setAnimation(float duration, avg::Curve curve,
            signal2::Signal<T, U> signal)
    {
        return setAnimation(avg::AnimationOptions{
                std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::duration<float>(duration)
                        ),
                std::move(curve)
                },
                std::move(signal)
                );
    }
} // namespace reactive::widget

