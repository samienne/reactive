#pragma once

#include "widget.h"

#include <bq/signal/signal.h>

#include <ase/hoverevent.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier onHover(
            bq::signal::AnySignal<std::function<void(reactive::HoverEvent const&)>> cb,
            bq::signal::AnySignal<avg::Obb> area);

    REACTIVE_EXPORT AnyWidgetModifier onHover(bq::signal::AnySignal<
            std::function<void(reactive::HoverEvent const&)>
            > cb);

    REACTIVE_EXPORT AnyWidgetModifier onHover(
            std::function<void(reactive::HoverEvent const&)> cb
            );

    REACTIVE_EXPORT AnyWidgetModifier onHover(bq::signal::InputHandle<bool> handle);


    REACTIVE_EXPORT AnyWidgetModifier onHover(bq::signal::AnySignal<avg::Obb> obb,
            bq::signal::InputHandle<bool> handle);
} // namespace reactive::widget

