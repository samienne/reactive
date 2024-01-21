#pragma once

#include "widget.h"

#include <reactive/signal/inputhandle.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier trackFocus(signal::InputHandle<bool> const& handle);
} // namespace reactive::widget

