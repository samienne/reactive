#pragma once

#include "widget.h"

#include "reactive/signal2/signal.h"

#include "reactive/eventresult.h"

#include <functional>

namespace reactive::widget
{
    AnyWidgetModifier onPointerUp(signal2::AnySignal<
            std::function<EventResult(ase::PointerButtonEvent const&)>
            > cb);

    AnyWidgetModifier onPointerUp(
            std::function<EventResult(ase::PointerButtonEvent const&)> cb);

    AnyWidgetModifier onPointerUp(std::function<EventResult()> cb);
} // namespace reactive::widget

