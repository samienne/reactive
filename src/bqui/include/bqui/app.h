#pragma once

#include "window.h"
#include "bquivisibility.h"

#include <bq/signal/arraysignal.h>
#include <bq/signal/signal.h>

#include <avg/curve/curves.h>

#include <ase/platform.h>

#include <btl/shared.h>
#include <btl/uniqueid.h>
#include <btl/visibility.h>

#include <optional>
#include <string>
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

        /** @brief Force a specific ase platform (e.g. a headless one),
         * bypassing env selection. Lets a test run headless without a
         * process-global env var.
         */
        App& platform(ase::Platform platform);

        /** @brief Force headless (dummy) or headful, overriding the
         * REACTIVE_HEADLESS / REACTIVE_PLATFORM env vars.
         */
        App& headless(bool headless);

        /** @brief Force agentic mode on/off, overriding the REACTIVE_AGENT /
         * REACTIVE_MODE env vars.
         *
         * Orthogonal to the platform choice. In agentic mode the app connects
         * to an endpoint and is driven by an external agent instead of
         * free-running.
         */
        App& agentic(bool agentic);

        /** @brief Set the agent channel endpoint, overriding
         * REACTIVE_AGENT_ENDPOINT, so a test need not set a process-global env.
         */
        App& agentEndpoint(std::string endpoint);

        /** @brief Opens a list of windows the caller drives.
         *
         * The general form, for a caller whose windows follow a model of its
         * own: the array is the source of truth for the windows in it, so they
         * open and close as its keys come and go, and every window that stays
         * keeps everything it had — its widgets, their state, and its own OS
         * window. The app does not copy such a list into its own collection,
         * and cannot: removing one of these windows means removing its key
         * from whatever the array was built from, so 'Window::close()' and
         * removeWindow() do nothing for them.
         *
         * Both kinds of window are open at once. The default run() stops on the
         * app's own collection alone, though, so an app whose windows are all in
         * an array wants a 'running' signal of its own — run() with no argument
         * would see an empty collection and return at once.
         *
         * @throws std::logic_error if the app is already running. Unlike the
         *         app's own collection, the set of arrays is fixed when run()
         *         starts, because they are joined once. Add windows while the
         *         app runs through addWindows(), or take them from an array
         *         that is already there.
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
         * bq::signal::forEach() keyed on 'Window::getId' to build something per
         * window.
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
         * Runs while the app's own collection has a window in it, and stops
         * when the last one is removed. This is a default 'running' signal
         * derived from that collection, not a rule of the loop: it counts only
         * the windows added through addWindows() and removeWindow(), not those
         * from addWindowArray(), and a caller that wants another policy —
         * counting array-supplied windows, or outliving an empty collection —
         * passes its own signal to run(running). An app with no window in its
         * own collection to begin with returns at once.
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

