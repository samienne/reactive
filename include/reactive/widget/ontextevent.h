#pragma once

#include "widget.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier onTextEvent(AnySignal<KeyboardInput::TextHandler> handler);

    REACTIVE_EXPORT AnyWidgetModifier onTextEvent(KeyboardInput::TextHandler handler);
} // namespace reactive::widget

