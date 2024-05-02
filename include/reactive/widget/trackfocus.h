#pragma once

#include "widget.h"

#include <reactive/signal2/input.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier trackFocus(
            signal2::InputHandle<bool> const& handle);
} // namespace reactive::widget

