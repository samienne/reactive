#pragma once

#include "bquivisibility.h"

#include <bq/signal/signal.h>

#include <btl/uniqueid.h>

#include <functional>
#include <memory>
#include <string>

namespace bqui
{
    class WindowData;
    class App;
    class AppDeferred;
    class WindowImpl;

    /** @brief A window the app owns, as a small value handle.
     *
     * A Window is a handle over shared state (its identity, title and close
     * callbacks): copies of a window are one window and share that state, and
     * the state lives as long as any copy does — across a remove and a later
     * re-add. The window holds none of its own contents; the widget is supplied
     * to App::addWindow at mount time and lives in the app-owned impl.
     *
     * Because of that, a widget inside the window may capture the very Window
     * that owns it — a close button is @c [w]{ w.close(); } — with no retain
     * cycle: the app holds the widget, the widget holds the Window, and the
     * Window points back at the app only weakly.
     */
    class BQUI_EXPORT Window
    {
    public:
        /** @brief Mints a window with a fresh identity, belonging to no app. */
        explicit Window(bq::signal::AnySignal<std::string> const& title);

        Window(Window const&) = default;
        Window& operator=(Window const&) = default;

        Window(Window&&) = default;
        Window& operator=(Window&&) = default;

        /** @brief Adds a callback run when the window's title bar closes it. */
        Window onClose(std::function<void()> const& cb) &&;

        bq::signal::AnySignal<std::string> const& getTitle() const;

        void invokeOnClose() const;

        /** @brief This window's identity, which its copies share.
         *
         * Two copies of one window are one window, so adding a copy to an app
         * that already holds the original is adding the same window twice, and
         * is rejected as such.
         */
        btl::UniqueId getId() const;

        /** @brief Removes this window from the app that owns it.
         *
         * A no-op if the window is in no app, or if that app is gone. Closing
         * twice is a race a UI can lose without anything being wrong.
         */
        void close() const;

        Window clone() const
        {
            return *this;
        }

    private:
        friend class App;
        friend class AppDeferred;
        friend class WindowImpl;

        std::shared_ptr<WindowData> const& data() const
        {
            return data_;
        }

        std::shared_ptr<WindowData> data_;
    };

    /** @brief Mints a window with the given title. */
    BQUI_EXPORT Window window(bq::signal::AnySignal<std::string> const& title);
}
