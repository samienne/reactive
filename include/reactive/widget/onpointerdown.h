#pragma once

#include "widget.h"

#include "reactive/eventresult.h"
#include "reactive/pointerbuttonevent.h"

#include "reactive/signal/signal.h"

#include <functional>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier onPointerDown(
            AnySignal<std::function<EventResult(ase::PointerButtonEvent const&)>> cb);

    REACTIVE_EXPORT AnyWidgetModifier onPointerDown(
            std::function<EventResult(ase::PointerButtonEvent const&)> cb
            );
} // namespace reactive::widget

