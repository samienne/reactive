#pragma once

#include "widget.h"

#include "reactive/eventresult.h"
#include "reactive/pointerbuttonevent.h"

#include "reactive/signal2/signal.h"

#include <functional>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier onPointerDown(
            signal2::AnySignal<std::function<EventResult(
                ase::PointerButtonEvent const&)
            >> cb);

    REACTIVE_EXPORT AnyWidgetModifier onPointerDown(
            std::function<EventResult(ase::PointerButtonEvent const&)> cb
            );
} // namespace reactive::widget

