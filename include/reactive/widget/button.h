#pragma once

#include "reactive/widget/widget.h"

#include "reactive/reactivevisibility.h"

#include "reactive/signal/signal.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidget button(signal::AnySignal<std::string> label,
            signal::AnySignal<std::function<void()>> onClick);

    REACTIVE_EXPORT AnyWidget button(std::string label,
            signal::AnySignal<std::function<void()>> onClick);
} // namespace reactive::widget

