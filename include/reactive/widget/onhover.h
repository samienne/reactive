#pragma once

#include "widget.h"

#include <reactive/signal/signal.h>

#include <ase/hoverevent.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier onHover(
            signal::AnySignal<std::function<void(reactive::HoverEvent const&)>> cb,
            signal::AnySignal<avg::Obb> area);

    REACTIVE_EXPORT AnyWidgetModifier onHover(signal::AnySignal<
            std::function<void(reactive::HoverEvent const&)>
            > cb);

    REACTIVE_EXPORT AnyWidgetModifier onHover(
            std::function<void(reactive::HoverEvent const&)> cb
            );

    REACTIVE_EXPORT AnyWidgetModifier onHover(signal::InputHandle<bool> handle);


    REACTIVE_EXPORT AnyWidgetModifier onHover(signal::AnySignal<avg::Obb> obb,
            signal::InputHandle<bool> handle);
} // namespace reactive::widget

