#pragma once

#include "window.h"
#include "reactivevisibility.h"

#include <bq/signal/signal.h>

#include <avg/curve/curves.h>

#include <btl/shared.h>
#include <btl/visibility.h>

namespace reactive
{
    class AppDeferred;
    class AnimationGuard;

    class REACTIVE_EXPORT App
    {
    public:
        explicit App();

        App windows(std::initializer_list<Window> windows) &&;

        int run(bq::signal::AnySignal<bool> running) &&;
        int run() &&;

        [[nodiscard]]
        AnimationGuard withAnimation(avg::AnimationOptions options);

        friend class AnimationGuard;

    private:
        inline AppDeferred* d()
        {
            return deferred_.get();
        }

        inline AppDeferred const* d() const
        {
            return deferred_.get();
        }

    private:
        btl::shared<AppDeferred> deferred_;
    };

    class REACTIVE_EXPORT AnimationGuard
    {
    public:
        AnimationGuard(AppDeferred& app, std::optional<avg::AnimationOptions> options);
        AnimationGuard(AnimationGuard const& rhs) noexcept = delete;
        AnimationGuard(AnimationGuard&& rhs) noexcept = delete;

        ~AnimationGuard();

        AnimationGuard& operator=(AnimationGuard const& rhs) noexcept = delete;
        AnimationGuard& operator=(AnimationGuard&& rhs) noexcept = delete;

    private:
        AppDeferred* app_ = nullptr;
        std::optional<avg::AnimationOptions> options_;
    };

    REACTIVE_EXPORT App app();
} // namespace reactive

