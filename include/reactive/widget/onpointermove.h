#pragma once

#include "widget.h"

#include "reactive/signal2/signal.h"

#include "reactive/eventresult.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier onPointerMove(signal2::AnySignal<
            std::function<EventResult(ase::PointerMoveEvent const&)>
            > cb);

    REACTIVE_EXPORT AnyWidgetModifier onPointerMove(
            std::function<EventResult(ase::PointerMoveEvent const&)> cb
            );
} // namespace reactive::widget

