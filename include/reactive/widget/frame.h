#pragma once

#include "widget.h"

#include <avg/color.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier frame(
            signal2::AnySignal<float> cornerRadius,
            signal2::AnySignal<avg::Color> color
            );

    REACTIVE_EXPORT AnyWidgetModifier frame(signal2::AnySignal<avg::Color> color);

    REACTIVE_EXPORT AnyWidgetModifier frame(signal2::AnySignal<float> cornerRadius);

    REACTIVE_EXPORT AnyWidgetModifier frame();
} // namespace reactive::widget

