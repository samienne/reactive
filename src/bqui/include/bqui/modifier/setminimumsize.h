#pragma once

#include "widgetmodifier.h"

#include "bqui/bquivisibility.h"

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier setMinimumSize(
            bq::signal::AnySignal<avg::Vector2f> size
            );

    BQUI_EXPORT AnyWidgetModifier setMinimumWidth(bq::signal::AnySignal<float> width);
    BQUI_EXPORT AnyWidgetModifier setMinimumHeight(bq::signal::AnySignal<float> height);
}
