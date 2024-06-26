#pragma once

#include "widget.h"

#include "reactive/sizehint.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier setSizeHint(
            signal::AnySignal<SizeHint>sizeHint);
} // namespace reactive::widget

