#pragma once

#include "reactive/widgetfactory.h"
#include "reactive/signal.h"
#include "reactive/reactivevisibility.h"
#include "reactive/signal/cast.h"

namespace reactive::widget
{
    REACTIVE_EXPORT WidgetFactory button(Signal<std::string> label,
            Signal<std::function<void()>> onClick);

    REACTIVE_EXPORT WidgetFactory button(std::string label,
            Signal<std::function<void()>> onClick);

    /*
    template <typename T, typename U, typename = std::enable_if<
        std::is_convertible_v<T, std::function<void()>>
        >>
    WidgetFactory button(std::string label, Signal<T, U> onClick)
    {
        return button(std::move(label), signal::cast<std::function<void()>>(
                    std::move(onClick)));
    }
    */
} // namespace reactive::widget

