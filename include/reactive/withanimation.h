#pragma once

#include "app.h"

#include <avg/animationoptions.h>

namespace reactive
{
    [[nodiscard]]
    AnimationGuard REACTIVE_EXPORT withAnimation(avg::AnimationOptions options);

    [[nodiscard]]
    AnimationGuard REACTIVE_EXPORT withAnimation(std::chrono::milliseconds time,
            avg::Curve curve = avg::curve::linear);

    [[nodiscard]]
    AnimationGuard REACTIVE_EXPORT withAnimation(float seconds,
            avg::Curve curve = avg::curve::linear);

    void REACTIVE_EXPORT withAnimation(avg::AnimationOptions animationOptions,
            std::function<void()> fn);

    void REACTIVE_EXPORT withAnimation(
            std::chrono::milliseconds duration,
            avg::Curve curve,
            std::function<void()> callback
            );

    void REACTIVE_EXPORT withAnimation(
            float seconds,
            avg::Curve curve,
            std::function<void()> callback
            );
} // namespace reactive

