#pragma once

#include "reactive/widget.h"

#include "reactive/widgetfactory.h"

namespace reactive::widget
{
    BTL_VISIBLE WidgetFactory hScrollBar(
            signal::InputHandle<float> handle,
            Signal<float> amount,
            Signal<float> handleSize);

    BTL_VISIBLE WidgetFactory vScrollBar(
            signal::InputHandle<float> handle,
            Signal<float> amount,
            Signal<float> handleSize);
} // namespace reactive::widget

