#pragma once

#include "widgetmodifier.h"

#include "bqui/eventresult.h"

#include <bq/signal/signal.h>

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier onPointerMove(bq::signal::AnySignal<
            std::function<EventResult(ase::PointerMoveEvent const&)>
            > cb);

    BQUI_EXPORT AnyWidgetModifier onPointerMove(
            std::function<EventResult(ase::PointerMoveEvent const&)> cb
            );
} // namespace reactive::widget

