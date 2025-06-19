#pragma once

#include "widget.h"

#include <bq/signal/input.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier trackFocus(
            bq::signal::InputHandle<bool> const& handle);
} // namespace reactive::widget

