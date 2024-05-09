#pragma once

#include "widget.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier setId(
            signal::AnySignal<avg::UniqueId> id);
} // namespace reactive::widget

