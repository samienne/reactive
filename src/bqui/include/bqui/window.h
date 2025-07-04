#pragma once

#include "widget/widget.h"

#include "bquivisibility.h"

#include <bq/signal/signal.h>

#include <btl/cloneoncopy.h>

namespace bqui
{
    class BQUI_EXPORT Window
    {
    public:
        Window(widget::AnyWidget widget,
                bq::signal::AnySignal<std::string> const& title);

        Window(Window const&) = default;
        Window& operator=(Window const&) = default;

        Window(Window&&) = default;
        Window& operator=(Window&&) = default;

        Window onClose(std::function<void()> const& cb) &&;

        widget::AnyWidget getWidget() const;

        bq::signal::AnySignal<std::string> const& getTitle() const;

        void invokeOnClose() const;

        Window clone() const
        {
            return *this;
        }

    private:
        widget::AnyWidget widget_;
        bq::signal::AnySignal<std::string> title_;
        std::vector<std::function<void()>> closeCallbacks_;
    };

    BQUI_EXPORT auto window(bq::signal::AnySignal<std::string> const& title,
            widget::AnyWidget widget) -> Window;
}

