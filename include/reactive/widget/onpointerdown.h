#pragma once

#include "widget.h"

#include "reactive/eventresult.h"
#include "reactive/pointerbuttonevent.h"

#include <bq/signal/signal.h>

#include <functional>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier onPointerDown(
            signal::AnySignal<std::function<EventResult(
                reactive::PointerButtonEvent const&)
            >> cb);

    REACTIVE_EXPORT AnyWidgetModifier onPointerDown(
            std::function<EventResult(reactive::PointerButtonEvent const&)> cb
            );
} // namespace reactive::widget

