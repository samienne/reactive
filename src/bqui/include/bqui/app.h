#pragma once

#include "window.h"
#include "bquivisibility.h"

#include <bq/signal/arraysignal.h>
#include <bq/signal/signal.h>

#include <avg/curve/curves.h>

#include <btl/shared.h>
#include <btl/visibility.h>

#include <optional>

namespace bqui
{
    class AppDeferred;
    class AnimationGuard;

    class BQUI_EXPORT App
    {
    public:
        explicit App();

        /** @brief Sets the windows the app opens.
         *
         * The list is the only thing that says which windows exist. A braced
         * list is a constant array, so `windows({ a, b })` opens two windows
         * and never changes; a list built with bq::signal::forEach() opens and
         * closes windows as its keys come and go, and every window that stays
         * keeps everything it had — its widgets, their state, and its own OS
         * window.
         *
         * A window closes by leaving the list, so wire what a window offers to
         * whatever the list is built from: `Window::onClose` on a window whose
         * key comes from an input removes that key.
         */
        App windows(bq::signal::ArraySignal<Window> windows) &&;

        /** @brief Runs until `running` is false. */
        int run(bq::signal::AnySignal<bool> running) &&;

        /** @overload
         *
         * Runs until a window closes — **any** window, including one that
         * opened later. That is what a single-window app wants; a list whose
         * windows come and go wants the overload above, so that closing one
         * window can mean removing it rather than stopping.
         */
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

