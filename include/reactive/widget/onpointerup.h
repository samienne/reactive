#pragma once

#include "setinputareas.h"
#include "instancemodifier.h"
#include "widget.h"

#include "reactive/signal/signal.h"

#include "reactive/pointerbuttonevent.h"
#include "reactive/eventresult.h"

#include <functional>
#include <type_traits>

namespace reactive::widget
{
    AnyWidgetModifier onPointerUp(
            AnySignal<std::function<EventResult(ase::PointerButtonEvent const&)>> cb);

    AnyWidgetModifier onPointerUp(
            std::function<EventResult(ase::PointerButtonEvent const&)> cb);

    AnyWidgetModifier onPointerUp(std::function<EventResult()> cb);
} // namespace reactive::widget

