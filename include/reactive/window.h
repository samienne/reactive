#pragma once

#include "widget/builder.h"

#include "reactivevisibility.h"

#include "signal/signal.h"

#include <btl/cloneoncopy.h>

namespace reactive
{
    class REACTIVE_EXPORT Window
    {
    public:
        Window(widget::Builder widget, AnySignal<std::string> const& title);

        Window(Window const&) = default;
        Window& operator=(Window const&) = default;

        Window(Window&&) = default;
        Window& operator=(Window&&) = default;

        Window onClose(std::function<void()> const& cb) &&;

        widget::Builder getWidget() const;

        AnySharedSignal<std::string> const& getTitle() const;

        void invokeOnClose() const;

        Window clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<widget::Builder> widget_;
        AnySharedSignal<std::string> title_;
        std::vector<std::function<void()>> closeCallbacks_;
    };

    REACTIVE_EXPORT auto window(AnySignal<std::string> const& title,
            widget::Builder widget) -> Window;
}

