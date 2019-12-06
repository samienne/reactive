#pragma once

#include "widgetfactory.h"

#include "signal.h"
#include "reactivevisibility.h"

#include <btl/cloneoncopy.h>

namespace reactive
{
    class REACTIVE_EXPORT Window
    {
    public:
        Window(WidgetFactory widget, AnySignal<std::string> const& title);

    private:
        Window(Window const&) = default;
        Window& operator=(Window const&) = default;

    public:
        Window(Window&&) = default;
        Window& operator=(Window&&) = default;

        Window onClose(std::function<void()> const& cb) &&;

        WidgetFactory getWidget() const;

        AnySignal<std::string> const& getTitle() const;

        void invokeOnClose() const;

        Window clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<WidgetFactory> widget_;
        AnySharedSignal<std::string> title_;
        std::vector<std::function<void()>> closeCallbacks_;
    };

    REACTIVE_EXPORT auto window(AnySignal<std::string> const& title,
            WidgetFactory widget) -> Window;
}

