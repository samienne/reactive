#pragma once

#include "widget.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier setId(AnySignal<avg::UniqueId> id);
} // namespace reactive::widget

