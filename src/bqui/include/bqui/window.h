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

    /** @brief A window in the application.
     *
     * Create a Window, add it to the App with the widget to show in it, and
     * close it when you are done.
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

        /** @brief This window's identity, which its copies share. */
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
