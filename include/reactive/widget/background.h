#pragma once

#include "widget.h"

#include "reactive/signal2/signal.h"

#include <avg/brush.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier background(AnyWidget bgWidget);

    REACTIVE_EXPORT AnyWidgetModifier background(
            signal2::AnySignal<avg::Brush> brush);

    REACTIVE_EXPORT AnyWidgetModifier background();
} // namespace reactive::widget

