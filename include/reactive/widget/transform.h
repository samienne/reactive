#pragma once

#include "widget.h"

#include <reactive/signal2/signal.h>

#include <avg/transform.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyInstanceModifier transformBuilder(
            signal2::AnySignal<avg::Transform> t);
    REACTIVE_EXPORT AnyWidgetModifier transform(
            signal2::AnySignal<avg::Transform> t);
} // namespace reactive::widget

