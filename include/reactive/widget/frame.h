#pragma once

#include "widget.h"

#include <avg/color.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier frame(
            bq::signal::AnySignal<float> cornerRadius,
            bq::signal::AnySignal<avg::Color> color
            );

    REACTIVE_EXPORT AnyWidgetModifier frame(bq::signal::AnySignal<avg::Color> color);

    REACTIVE_EXPORT AnyWidgetModifier frame(bq::signal::AnySignal<float> cornerRadius);

    REACTIVE_EXPORT AnyWidgetModifier frame();
} // namespace reactive::widget

