#pragma once

#include "signal/signal.h"

#include "window.h"
#include "reactivevisibility.h"

#include <btl/shared.h>
#include <btl/visibility.h>
#include <chrono>

namespace reactive
{
    class AppDeferred;

    class REACTIVE_EXPORT App
    {
    public:
        explicit App();

        App windows(std::initializer_list<Window> windows) &&;

        int run(AnySignal<bool> running) &&;
        int run() &&;

        void withAnimation(avg::AnimationOptions animationOptions,
                std::function<void()> fn);

        void withAnimation(
                std::chrono::milliseconds duration,
                std::function<float(float)> curve,
                std::function<void()> callback
                );

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

    REACTIVE_EXPORT App app();
} // namespace reactive

