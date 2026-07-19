#pragma once

#include "window.h"
#include "bquivisibility.h"

#include <bq/signal/signal.h>

#include <avg/curve/curves.h>

#include <btl/shared.h>
#include <btl/visibility.h>

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace bqui
{
    class AppDeferred;
    class AnimationGuard;

    /** A reactive window list: each window paired with a stable id.
     *
     * The id is assigned by the producer and is stable across edits. The run
     * loop reconciles the live window set by id — creating a window when its id
     * appears and destroying it when the id disappears — mirroring the
     * dynamic-widget list produced by `dataBind`.
     */
    using WindowList =
        bq::signal::AnySignal<std::vector<std::pair<size_t, Window>>>;

    class BQUI_EXPORT App
    {
    public:
        explicit App();

        App windows(std::initializer_list<Window> windows) &&;

        App windows(WindowList windows) &&;

        /** Observe the live window set after each reconciliation.
         *
         * The callback fires once at startup and again whenever the window
         * list changes, receiving the ids of the currently live windows in
         * list order. Intended for headless introspection and testing.
         */
        App onWindowsReconciled(
                std::function<void(std::vector<size_t> const&)> callback) &&;

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

