#pragma once

#include "widgetmodifier.h"

#include <bq/signal/signal.h>

#include <ase/hoverevent.h>

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier onHover(
            bq::signal::AnySignal<void(HoverEvent const&)> cb,
            bq::signal::AnySignal<avg::Obb> area);

    BQUI_EXPORT AnyWidgetModifier onHover(
            bq::signal::AnySignal<void(HoverEvent const&)> cb);

    BQUI_EXPORT AnyWidgetModifier onHover(bq::signal::InputHandle<bool> handle);


    BQUI_EXPORT AnyWidgetModifier onHover(bq::signal::AnySignal<avg::Obb> obb,
            bq::signal::InputHandle<bool> handle);
}

