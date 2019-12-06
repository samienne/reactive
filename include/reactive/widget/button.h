#pragma once

#include "reactive/widgetfactory.h"
#include "reactive/signal.h"
#include "reactive/reactivevisibility.h"
#include "reactive/signal/cast.h"

namespace reactive::widget
{
    REACTIVE_EXPORT WidgetFactory button(AnySignal<std::string> label,
            AnySignal<std::function<void()>> onClick);

    REACTIVE_EXPORT WidgetFactory button(std::string label,
            AnySignal<std::function<void()>> onClick);
} // namespace reactive::widget

