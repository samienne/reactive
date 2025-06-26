#pragma once

#include "window.h"
#include "bquivisibility.h"

#include <bq/signal/signal.h>

#include <avg/curve/curves.h>

#include <btl/shared.h>
#include <btl/visibility.h>

namespace bqui
{
    class AppDeferred;
    class AnimationGuard;

    class BQUI_EXPORT App
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

    class BQUI_EXPORT AnimationGuard
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

    BQUI_EXPORT App app();
}

