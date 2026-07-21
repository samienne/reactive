#pragma once

#include "widget/widget.h"

#include "bquivisibility.h"

#include <bq/signal/signal.h>

#include <btl/cloneoncopy.h>
#include <btl/uniqueid.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace bqui
{
    class WindowList;

    /** @brief A window's identity, and the means to close it.
     *
     * Every Window has one, and copies of a window share it. It is small and
     * holds nothing of the window's contents, so a widget *inside* the window
     * can hold one — construct the handle first, capture it in the widget, and
     * pass it to window(). A window that captured itself instead would own the
     * widget that owns it.
     *
     * A handle only reaches the app the window was added to, and reaches it
     * weakly: a handle for a window that was never added, or whose app has been
     * destroyed, still answers getId() and does nothing on close().
     */
    class BQUI_EXPORT WindowHandle
    {
    public:
        /** @brief Mints a fresh identity, belonging to no app. */
        WindowHandle();

        WindowHandle(WindowHandle const&) = default;
        WindowHandle& operator=(WindowHandle const&) = default;

        WindowHandle(WindowHandle&&) noexcept = default;
        WindowHandle& operator=(WindowHandle&&) noexcept = default;

        ~WindowHandle();

        /** @brief The identity this handle names, for App::removeWindow(). */
        btl::UniqueId getId() const;

        /** @brief Removes the window from the app that owns it.
         *
         * Does nothing if the window is not in an app, or if the app is gone.
         * Removing a window that is not there is not an error either — closing
         * twice is a race a UI can lose without anything being wrong.
         *
         * Only the app's own collection is reached. A window supplied through
         * an ArraySignal belongs to whatever the caller built that array from,
         * and comes out of the caller's own state.
         */
        void close() const;

    private:
        friend class WindowList;

        void setList(std::weak_ptr<WindowList> list) const;

        struct Impl;

        std::shared_ptr<Impl> impl_;
    };

    class BQUI_EXPORT Window
    {
    public:
        Window(widget::AnyWidget widget,
                bq::signal::AnySignal<std::string> const& title);

        /** @brief Constructs a window with an identity minted beforehand. */
        Window(widget::AnyWidget widget,
                bq::signal::AnySignal<std::string> const& title,
                WindowHandle handle);

        Window(Window const&) = default;
        Window& operator=(Window const&) = default;

        Window(Window&&) = default;
        Window& operator=(Window&&) = default;

        Window onClose(std::function<void()> const& cb) &&;

        widget::AnyWidget getWidget() const;

        bq::signal::AnySignal<std::string> const& getTitle() const;

        void invokeOnClose() const;

        /** @brief This window's identity, which its copies share.
         *
         * Two copies of one window are one window, so adding a copy to an app
         * that already holds the original is adding the same window twice, and
         * is rejected as such.
         */
        btl::UniqueId getId() const;

        WindowHandle const& getHandle() const;

        /** @brief Removes this window from the app that owns it. */
        void close() const;

        Window clone() const
        {
            return *this;
        }

    private:
        friend class WindowList;

        widget::AnyWidget widget_;
        bq::signal::AnySignal<std::string> title_;
        std::vector<std::function<void()>> closeCallbacks_;
        WindowHandle handle_;
    };

    BQUI_EXPORT auto window(bq::signal::AnySignal<std::string> const& title,
            widget::AnyWidget widget) -> Window;

    /** @overload */
    BQUI_EXPORT auto window(bq::signal::AnySignal<std::string> const& title,
            widget::AnyWidget widget, WindowHandle handle) -> Window;
}

