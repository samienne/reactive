#pragma once

#include "widget.h"

#include "reactive/widgetfactory.h"
#include "reactive/reactivevisibility.h"

namespace reactive::widget
{
    REACTIVE_EXPORT WidgetFactory hScrollBar(
            signal::InputHandle<float> handle,
            AnySignal<float> amount,
            AnySignal<float> handleSize);

    REACTIVE_EXPORT WidgetFactory vScrollBar(
            signal::InputHandle<float> handle,
            AnySignal<float> amount,
            AnySignal<float> handleSize);
} // namespace reactive::widget

