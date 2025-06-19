#pragma once

#include "widget.h"

#include "reactive/eventresult.h"

#include <bq/signal/signal.h>

#include <functional>

namespace reactive::widget
{
    AnyWidgetModifier onPointerUp(signal::AnySignal<
            std::function<EventResult(ase::PointerButtonEvent const&)>
            > cb);

    AnyWidgetModifier onPointerUp(
            std::function<EventResult(ase::PointerButtonEvent const&)> cb);

    AnyWidgetModifier onPointerUp(std::function<EventResult()> cb);
} // namespace reactive::widget

