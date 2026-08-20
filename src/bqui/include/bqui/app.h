#pragma once

#include "window.h"
#include "widget/widget.h"
#include "bquivisibility.h"

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

        /** @brief Opens a window the app owns, mounting the given widget in it.
         *
         * The widget becomes the window's mounted content, not part of its
         * identity: removing the window destroys the widget, and adding the same
         * window again re-supplies a fresh one. The collection is imperative, so
         * a window may be added and removed both before and while the app runs.
         *
         * @throws std::invalid_argument if the window is already open, here or
         *         in another app. A window and its copies are one window, so
         *         adding a copy of an open window is adding it twice. A removed
         *         window belongs to no app again and may be opened anywhere.
         */
        App& addWindow(Window window, widget::AnyWidget widget);

        /** @brief Runs with no visible windows, rendering each window's content
         * offscreen with the real backend instead of showing it.
         *
         * Orthogonal to the platform: the native GPU backend is still used, only
         * offscreen. Overrides the REACTIVE_HEADLESS environment variable. Set
         * before run(). To drive windows with no GPU at all -- e.g. where no GPU
         * is available, or to compile and run tests on otherwise-unsupported
         * platforms -- select the dummy platform explicitly via platform() or
         * REACTIVE_PLATFORM=dummy.
         */
        App& headless(bool headless);

        /** @brief Force a specific ase platform (e.g. the dummy one),
         * bypassing env selection. Lets a test drive windows with no GPU
         * without a process-global env var.
         */
        App& platform(ase::Platform platform);

        /** @brief Closes the app's window with this identity.
         *
         * Does nothing if no window in the collection has it. The window's own
         * data outlives this whenever a Window naming it is still held; only its
         * mounted widget is torn down.
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
         * The reactive form of the same collection, for a UI that follows it.
         */
        bq::signal::AnySignal<std::vector<Window>> getWindowsSignal() const;

        /** @brief Runs until 'running' is false, whatever the windows do.
         *
         * The calling thread is the app's thread: run() and withAnimation()
         * belong to it, and every window is built, drawn and driven there. The
         * window collection is the exception — addWindow(), removeWindow(),
         * getWindows() and Window::close() are lock-guarded, so a worker thread
         * can open or close a window itself.
         */
        int run(bq::signal::AnySignal<bool> running);

        /** @overload
         *
         * Runs until the last window is closed.
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

