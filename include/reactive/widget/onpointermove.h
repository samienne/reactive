#pragma once

#include "widget.h"

#include "reactive/signal/signal.h"

#include "reactive/eventresult.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier onPointerMove(signal::AnySignal<
            std::function<EventResult(ase::PointerMoveEvent const&)>
            > cb);

    REACTIVE_EXPORT AnyWidgetModifier onPointerMove(
            std::function<EventResult(ase::PointerMoveEvent const&)> cb
            );
} // namespace reactive::widget

