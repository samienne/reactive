#pragma once

#include "widget.h"

#include "reactive/clickevent.h"

#include "reactive/signal/signal.h"

#include <functional>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier onClick(unsigned int button,
            signal::AnySignal<std::function<void(ClickEvent const&)>> cb);

    REACTIVE_EXPORT AnyWidgetModifier onClick(unsigned int button,
            signal::AnySignal<std::function<void()>> cb);

    REACTIVE_EXPORT AnyWidgetModifier onClick(unsigned int button,
            std::function<void(ClickEvent const&)> f);

} // namespace reactive

