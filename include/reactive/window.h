#pragma once

#include "widgetfactory.h"

#include "reactivevisibility.h"

#include "signal/signal.h"

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
        Window(Window&&) noexcept = default;
        Window& operator=(Window&&) noexcept = default;

        Window onClose(std::function<void()> const& cb) &&;

        WidgetFactory getWidget() const;

        AnySharedSignal<std::string> const& getTitle() const;

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

    static_assert(std::is_nothrow_move_constructible_v<Window>, "");
    static_assert(std::is_nothrow_move_assignable_v<Window>, "");


    REACTIVE_EXPORT auto window(AnySignal<std::string> const& title,
            WidgetFactory widget) -> Window;
}

