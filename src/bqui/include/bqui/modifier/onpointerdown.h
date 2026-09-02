#pragma once

#include "widgetmodifier.h"

#include "bqui/eventresult.h"
#include "bqui/pointerbuttonevent.h"

#include <bq/signal/signal.h>

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier onPointerDown(
            bq::signal::AnySignal<EventResult(PointerButtonEvent const&)> cb);
}

