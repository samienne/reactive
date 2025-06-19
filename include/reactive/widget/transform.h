#pragma once

#include "widget.h"

#include <bq/signal/signal.h>

#include <avg/transform.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyInstanceModifier transformBuilder(
            bq::signal::AnySignal<avg::Transform> t);
    REACTIVE_EXPORT AnyWidgetModifier transform(
            bq::signal::AnySignal<avg::Transform> t);
} // namespace reactive::widget

