#pragma once

#include "widget.h"

#include <reactive/stream/stream.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier focusOn(stream::Stream<bool> stream);
} // namespace reactive::widget

