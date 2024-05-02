#pragma once

#include "widget.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier onTextEvent(
            signal2::AnySignal<KeyboardInput::TextHandler> handler);

    REACTIVE_EXPORT AnyWidgetModifier onTextEvent(KeyboardInput::TextHandler handler);
} // namespace reactive::widget

