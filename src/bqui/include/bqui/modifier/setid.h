#pragma once

#include "widgetmodifier.h"

namespace bqui::modifier
{
    BQUI_EXPORT AnyElementModifier setElementId(
            bq::signal::AnySignal<avg::UniqueId> id);
    BQUI_EXPORT AnyWidgetModifier setId(
            bq::signal::AnySignal<avg::UniqueId> id);
}

