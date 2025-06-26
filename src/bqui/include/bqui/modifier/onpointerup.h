#pragma once

#include "widgetmodifier.h"

#include "bqui/eventresult.h"

#include <bq/signal/signal.h>

#include <functional>

namespace bqui::modifier
{
    AnyWidgetModifier onPointerUp(bq::signal::AnySignal<
            std::function<EventResult(ase::PointerButtonEvent const&)>
            > cb);

    AnyWidgetModifier onPointerUp(
            std::function<EventResult(ase::PointerButtonEvent const&)> cb);

    AnyWidgetModifier onPointerUp(std::function<EventResult()> cb);
}

