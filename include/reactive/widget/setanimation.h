#pragma once

#include "setparams.h"

#include <avg/curve.h>
#include <avg/animationoptions.h>

namespace reactive::widget
{
    struct AnimationTag
    {
        using type = std::optional<avg::AnimationOptions>;
        static AnySharedSignal<type> const defaultValue;
    };

    AnySharedSignal<typename AnimationTag::type> const AnimationTag::defaultValue =
        share(signal::constant<typename AnimationTag::type>(std::nullopt));

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
} // namespace reactive::widget

