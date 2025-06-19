#pragma once

#include "widget.h"

#include <bq/signal/signal.h>

#include <avg/transform.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyInstanceModifier transformBuilder(
            signal::AnySignal<avg::Transform> t);
    REACTIVE_EXPORT AnyWidgetModifier transform(
            signal::AnySignal<avg::Transform> t);
} // namespace reactive::widget

