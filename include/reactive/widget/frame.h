#pragma once

#include "widget.h"

#include <avg/color.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier frame(
            signal::AnySignal<float> cornerRadius,
            signal::AnySignal<avg::Color> color
            );

    REACTIVE_EXPORT AnyWidgetModifier frame(signal::AnySignal<avg::Color> color);

    REACTIVE_EXPORT AnyWidgetModifier frame(signal::AnySignal<float> cornerRadius);

    REACTIVE_EXPORT AnyWidgetModifier frame();
} // namespace reactive::widget

