#pragma once

#include "widgetmodifier.h"

#include "bqui/bquivisibility.h"

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier setMaximumSize(
            bq::signal::AnySignal<avg::Vector2f> size
            );

    BQUI_EXPORT AnyWidgetModifier setMaximumSize(avg::Vector2f size);

    BQUI_EXPORT AnyWidgetModifier setMaximumWidth(bq::signal::AnySignal<float> width);
    BQUI_EXPORT AnyWidgetModifier setMaximumWidth(float width);
    BQUI_EXPORT AnyWidgetModifier setMaximumHeight(bq::signal::AnySignal<float> height);
    BQUI_EXPORT AnyWidgetModifier setMaximumHeight(float height);
}
