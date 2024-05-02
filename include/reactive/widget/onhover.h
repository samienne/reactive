#pragma once

#include "widget.h"

#include <reactive/signal2/signal.h>

#include <ase/hoverevent.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier onHover(
            signal2::AnySignal<std::function<void(reactive::HoverEvent const&)>> cb,
            signal2::AnySignal<avg::Obb> area);

    REACTIVE_EXPORT AnyWidgetModifier onHover(signal2::AnySignal<
            std::function<void(reactive::HoverEvent const&)>
            > cb);

    REACTIVE_EXPORT AnyWidgetModifier onHover(
            std::function<void(reactive::HoverEvent const&)> cb
            );

    REACTIVE_EXPORT AnyWidgetModifier onHover(signal2::InputHandle<bool> handle);


    REACTIVE_EXPORT AnyWidgetModifier onHover(signal2::AnySignal<avg::Obb> obb,
            signal2::InputHandle<bool> handle);
} // namespace reactive::widget

