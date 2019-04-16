#pragma once

#include "reactive/widget.h"

#include "reactive/widgetfactory.h"
#include "reactive/reactivevisibility.h"

namespace reactive::widget
{
    REACTIVE_EXPORT WidgetFactory hScrollBar(
            signal::InputHandle<float> handle,
            Signal<float> amount,
            Signal<float> handleSize);

    REACTIVE_EXPORT WidgetFactory vScrollBar(
            signal::InputHandle<float> handle,
            Signal<float> amount,
            Signal<float> handleSize);
} // namespace reactive::widget

