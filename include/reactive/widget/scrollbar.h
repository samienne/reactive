#pragma once

#include "reactive/widget.h"

#include "reactive/widgetfactory.h"

namespace reactive::widget
{
    BTL_VISIBLE WidgetFactory hScrollBar(
            signal::InputHandle<float> handle,
            Signal<float> amount);
} // namespace reactive::widget

