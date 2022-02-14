#pragma once

#include "setparams.h"
#include "withparams.h"

#include <reactive/signal/changed.h>

#include <avg/curve.h>
#include <avg/animationoptions.h>

namespace reactive::widget
{
    struct AnimationTag
    {
        using type = std::optional<avg::AnimationOptions>;
        static AnySharedSignal<type> getDefaultValue()
        {
            return share(signal::constant<typename AnimationTag::type>(std::nullopt));
        }
    };

    template <typename T>
    auto setAnimation(Signal<T, std::optional<avg::AnimationOptions>> animationOptions)
    {
        return setParams<AnimationTag>(share(std::move(animationOptions)));
    }

    inline auto setAnimation(avg::AnimationOptions options)
    {
        return setAnimation(
                signal::constant<std::optional<avg::AnimationOptions>>(
                    std::move(options)
                    )
                );
    }

    template <typename T, typename U>
    auto setAnimation(avg::AnimationOptions options, Signal<T, U> signal)
    {
        return withParams<AnimationTag>(
                [](auto widget, auto animation, auto options, auto signal)
                {
                    return std::move(widget)
                        | setAnimation(
                            group(changed(std::move(signal)), std::move(animation))
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
                std::move(options),
                std::move(signal)
                );
    }

    inline auto setAnimation(std::nullopt_t)
    {
        return setAnimation(
                signal::constant<std::optional<avg::AnimationOptions>>(
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
    auto setAnimation(float duration, avg::Curve curve, Signal<T, U> signal)
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

