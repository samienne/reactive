#pragma once

#include "widgetfactory.h"

#include <btl/cloneoncopy.h>

namespace reactive
{
    class Window
    {
    public:
        Window(WidgetFactory widget, Signal<std::string> const& title);

    private:
        Window(Window const&) = default;
        Window& operator=(Window const&) = default;

    public:
        Window(Window&&) = default;
        Window& operator=(Window&&) = default;

        Window onClose(std::function<void()> const& cb) &&;

        WidgetFactory getWidget() const;

        Signal<std::string> const& getTitle() const;

        void invokeOnClose() const;

        Window clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<WidgetFactory> widget_;
        Signal<std::string> title_;
        std::vector<std::function<void()>> closeCallbacks_;
    };

    auto window(Signal<std::string> const& title, WidgetFactory widget)
        -> Window;
}

