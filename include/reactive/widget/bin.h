#pragma once

#include "widget.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier bin(AnyBuilder f,
            AnySignal<avg::Vector2f> contentSize);
} // reactive::widget

