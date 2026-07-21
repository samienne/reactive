#pragma once

#include "window.h"
#include "bquivisibility.h"

#include <bq/signal/arraysignal.h>
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
         * Appends to the app's own collection, which is the list of windows
         * the app manages: a window in it closes when it is removed, whether
         * by Window::close(), by removeWindow(), or by its own title bar.
         * Windows may be added while the app runs.
         *
         * @throws std::invalid_argument if a window is already open. A window
         *         and its copies are one window — they share one identity — so
         *         adding a copy of an open window is adding it twice.
         */
        App& addWindows(std::vector<Window> windows);

        /** @overload */
        App& addWindow(Window window);

        /** @brief Opens a list of windows the caller drives.
         *
         * The general form, for a caller whose windows follow a model of its
         * own: the array is the source of truth for the windows in it, so they
         * open and close as its keys come and go, and every window that stays
         * keeps everything it had — its widgets, their state, and its own OS
         * window. The app does not copy such a list into its own collection,
         * and cannot: removing one of these windows means removing its key
         * from whatever the array was built from, so `Window::close()` and
         * removeWindow() do nothing for them.
         *
         * Both kinds of window are open at once, and run() counts them all.
         */
        App& addWindowArray(bq::signal::ArraySignal<Window> windows);

        /** @brief Closes the app's window with this identity.
         *
         * Does nothing if no window in the app's own collection has it.
         */
        void removeWindow(btl::UniqueId id);

        /** @brief The app's own windows as they are right now.
         *
         * The snapshot form, for code outside a signal graph — counting the
         * open windows, or finding one to remove. Windows added through
         * addWindowArray() are not here; they are the caller's to enumerate.
         */
        std::vector<Window> getWindows() const;

        /** @brief The app's own windows, as a signal.
         *
         * The reactive form of the same collection, for building a UI that
         * follows it — a window list, or a title that counts. Feed it to
         * bq::signal::forEach() keyed on `Window::getId` to build something per
         * window.
         */
        bq::signal::AnySignal<std::vector<Window>> getWindowsSignal() const;

        /** @brief Runs until `running` is false, whatever the windows do. */
        int run(bq::signal::AnySignal<bool> running);

        /** @overload
         *
         * Runs until no window is open — every window, from either kind of
         * list. Returns immediately if none is open to begin with.
         */
        int run();

        [[nodiscard]]
        AnimationGuard withAnimation(avg::AnimationOptions options);

        friend class AnimationGuard;

    private:
        int runUntil(std::optional<bq::signal::AnySignal<bool>> running);

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

