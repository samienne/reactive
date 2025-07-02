#pragma once

#include "widgetmodifier.h"

#include "bqui/bquivisibility.h"

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier setGravity(
            bq::signal::AnySignal<avg::Vector2f> gravity);

    BQUI_EXPORT AnyWidgetModifier setGravity(avg::Vector2f gravity);
}

