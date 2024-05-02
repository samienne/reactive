#pragma once

#include "widget.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier margin(signal2::AnySignal<float> amount);
    REACTIVE_EXPORT AnyWidgetModifier margin(float amount);
} // reactive::widget

