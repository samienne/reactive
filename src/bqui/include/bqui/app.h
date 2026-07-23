#pragma once

#include "window.h"
#include "bquivisibility.h"

#include <bq/signal/signal.h>

#include <avg/curve/curves.h>

#include <btl/shared.h>
#include <btl/uniqueid.h>
#include <btl/visibility.h>

#include <optional>
#include <vector>

namespace bqui
{
    class AppDeferred;
    class AnimationGuard;

    class BQUI_EXPORT App
    {
    public:
        explicit App();

        /** @brief Opens windows the app owns.
         *
         * Appends to the app's collection, which is the list of windows the app
         * manages: a window in it closes when it is removed, whether by
         * Window::close(), by removeWindow(), or by its own title bar. The
         * collection is an imperative one — added to and removed from directly,
         * not derived from a signal — so windows may be added and removed both
         * before and while the app runs.
         *
         * @throws std::invalid_argument if a window is already open, here or
         *         in another app. A window and its copies are one window —
         *         they share one identity — so adding a copy of an open window
         *         is adding it twice, and a window belongs to one app because
         *         close() has to know which list to leave. A window that has
         *         been removed belongs to no app again and may be opened
         *         anywhere.
         */
        App& addWindows(std::vector<Window> windows);

        /** @overload */
        App& addWindow(Window window);

        /** @brief Closes the app's window with this identity.
         *
         * Does nothing if no window in the collection has it.
         */
        void removeWindow(btl::UniqueId id);

        /** @brief The app's windows as they are right now.
         *
         * The snapshot form, for code outside a signal graph — counting the
         * open windows, or finding one to remove.
         */
        std::vector<Window> getWindows() const;

        /** @brief The app's windows, as a signal.
         *
         * The reactive form of the same collection, for building a UI that
         * follows it — a window list, or a title that counts. This is an
         * observation of the imperative collection, not its source: adding and
         * removing windows still goes through addWindows()/removeWindow()/
         * Window::close(), and this signal reports the result.
         */
        bq::signal::AnySignal<std::vector<Window>> getWindowsSignal() const;

        /** @brief Runs until 'running' is false, whatever the windows do.
         *
         * The thread that calls this is the app's thread: run() and
         * withAnimation() belong to it, and every window is built, drawn and
         * driven there. The window collection is the exception and is safe to
         * reach from anywhere — addWindows(), removeWindow(), getWindows() and
         * Window::close() all go through a lock-guarded vector, so a worker
         * that finishes can open or close a window itself.
         */
        int run(bq::signal::AnySignal<bool> running);

        /** @overload
         *
         * Runs while the collection has a window in it, and stops when the last
         * one is removed. This is a default 'running' signal derived from that
         * collection, not a rule of the loop: a caller that wants another policy
         * — outliving an empty collection, say — passes its own signal to
         * run(running). An app with no window to begin with returns at once.
         */
        int run();

        [[nodiscard]]
        AnimationGuard withAnimation(avg::AnimationOptions options);

        friend class AnimationGuard;

    private:
        int runUntil(bq::signal::AnySignal<bool> running);

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

