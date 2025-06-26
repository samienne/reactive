#pragma once

#include "widgetmodifier.h"

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier margin(bq::signal::AnySignal<float> amount);
    BQUI_EXPORT AnyWidgetModifier margin(float amount);
}

