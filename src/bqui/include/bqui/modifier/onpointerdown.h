#pragma once

#include "widgetmodifier.h"

#include "bqui/eventresult.h"
#include "bqui/pointerbuttonevent.h"

#include <bq/signal/signal.h>

#include <functional>

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier onPointerDown(
            bq::signal::AnySignal<std::function<EventResult(
                PointerButtonEvent const&)
            >> cb);

    BQUI_EXPORT AnyWidgetModifier onPointerDown(
            std::function<EventResult(PointerButtonEvent const&)> cb
            );
}

