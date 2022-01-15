#pragma once

#include "widget/widget.h"

#include "reactivevisibility.h"

#include "signal/signal.h"

#include <btl/cloneoncopy.h>

namespace reactive
{
    class REACTIVE_EXPORT Window
    {
    public:
        Window(widget::AnyWidget widget, AnySignal<std::string> const& title);

        Window(Window const&) = default;
        Window& operator=(Window const&) = default;

        Window(Window&&) = default;
        Window& operator=(Window&&) = default;

        Window onClose(std::function<void()> const& cb) &&;

        widget::AnyWidget getWidget() const;

        AnySharedSignal<std::string> const& getTitle() const;

        void invokeOnClose() const;

        Window clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<widget::AnyWidget> widget_;
        AnySharedSignal<std::string> title_;
        std::vector<std::function<void()>> closeCallbacks_;
    };

    REACTIVE_EXPORT auto window(AnySignal<std::string> const& title,
            widget::AnyWidget widget) -> Window;
}

