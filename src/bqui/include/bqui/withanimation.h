#pragma once

#include "app.h"

#include <avg/animationoptions.h>

namespace bqui
{
    [[nodiscard]]
    AnimationGuard BQUI_EXPORT withAnimation(avg::AnimationOptions options);

    [[nodiscard]]
    AnimationGuard BQUI_EXPORT withAnimation(std::chrono::milliseconds time,
            avg::Curve curve = avg::curve::linear);

    [[nodiscard]]
    AnimationGuard BQUI_EXPORT withAnimation(float seconds,
            avg::Curve curve = avg::curve::linear);

    void BQUI_EXPORT withAnimation(avg::AnimationOptions animationOptions,
            std::function<void()> fn);

    void BQUI_EXPORT withAnimation(
            std::chrono::milliseconds duration,
            avg::Curve curve,
            std::function<void()> callback
            );

    void BQUI_EXPORT withAnimation(
            float seconds,
            avg::Curve curve,
            std::function<void()> callback
            );
}

