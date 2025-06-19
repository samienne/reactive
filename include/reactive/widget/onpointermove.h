#pragma once

#include "widget.h"

#include "reactive/eventresult.h"

#include <bq/signal/signal.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier onPointerMove(bq::signal::AnySignal<
            std::function<EventResult(ase::PointerMoveEvent const&)>
            > cb);

    REACTIVE_EXPORT AnyWidgetModifier onPointerMove(
            std::function<EventResult(ase::PointerMoveEvent const&)> cb
            );
} // namespace reactive::widget

