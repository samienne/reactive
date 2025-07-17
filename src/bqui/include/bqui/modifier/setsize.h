#pragma once

#include "widgetmodifier.h"

#include <bqui/bquivisibility.h>

#include <avg/vector.h>

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier setSize(
            bq::signal::AnySignal<avg::Vector2f> size);

    BQUI_EXPORT AnyWidgetModifier setSize(avg::Vector2f size);
}

