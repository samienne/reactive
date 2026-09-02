#pragma once

#include "widgetmodifier.h"

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier onTextEvent(
            bq::signal::AnySignal<KeyboardInput::TextHandler> handler);
}

