#pragma once

#include "widget.h"

#include <avg/color.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier frame(
            AnySignal<float> cornerRadius,
            AnySignal<avg::Color> color
            );

    REACTIVE_EXPORT AnyWidgetModifier frame(AnySignal<avg::Color> color);

    REACTIVE_EXPORT AnyWidgetModifier frame(AnySignal<float> cornerRadius);

    REACTIVE_EXPORT AnyWidgetModifier frame();
} // namespace reactive::widget

