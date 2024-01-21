#pragma once

#include "widget.h"

#include <reactive/signal/signal.h>

#include <avg/transform.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyInstanceModifier transformBuilder(AnySignal<avg::Transform> t);
    REACTIVE_EXPORT AnyWidgetModifier transform(AnySignal<avg::Transform> t);
} // namespace reactive::widget

