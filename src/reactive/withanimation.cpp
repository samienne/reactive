#include "withanimation.h"

namespace reactive
{
    void withAnimation(avg::AnimationOptions animationOptions,
            std::function<void()> fn)
    {
        auto anim = withAnimation(std::move(animationOptions));

        fn();
    }

    void withAnimation(
            std::chrono::milliseconds duration,
            avg::Curve curve,
            std::function<void()> callback
            )
    {
        withAnimation(
                avg::AnimationOptions{ duration, std::move(curve) },
                std::move(callback)
                );
    }

    void withAnimation(
            float seconds,
            avg::Curve curve,
            std::function<void()> callback
            )
    {
        withAnimation(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::duration<float>(seconds)
                    ),
                std::move(curve),
                std::move(callback)
                );
    }

    AnimationGuard withAnimation(avg::AnimationOptions options)
    {
        return app().withAnimation(std::move(options));
    }

    AnimationGuard withAnimation(std::chrono::milliseconds time,
            avg::Curve curve)
    {
        return withAnimation({ time, std::move(curve)});
    }

    AnimationGuard withAnimation(float seconds,
            avg::Curve curve)
    {
        return withAnimation({
                std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::duration<float>(seconds)
                        ),
                std::move(curve)
                });
    }
} // namespace reactive
