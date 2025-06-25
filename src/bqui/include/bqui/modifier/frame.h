#pragma once

#include "widgetmodifier.h"

#include <avg/color.h>

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier frame(
            bq::signal::AnySignal<float> cornerRadius,
            bq::signal::AnySignal<avg::Color> color
            );

    BQUI_EXPORT AnyWidgetModifier frame(bq::signal::AnySignal<avg::Color> color);

    BQUI_EXPORT AnyWidgetModifier frame(bq::signal::AnySignal<float> cornerRadius);

    BQUI_EXPORT AnyWidgetModifier frame();
}

