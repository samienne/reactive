#pragma once

#include "widget.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier setId(
            signal2::AnySignal<avg::UniqueId> id);
} // namespace reactive::widget

