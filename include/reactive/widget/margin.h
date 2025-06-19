#pragma once

#include "widget.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier margin(bq::signal::AnySignal<float> amount);
    REACTIVE_EXPORT AnyWidgetModifier margin(float amount);
} // reactive::widget

