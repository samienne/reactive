#pragma once

#include "widgetmodifier.h"

#include <bq/signal/input.h>

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier trackFocus(
            bq::signal::InputHandle<bool> const& handle);
}

