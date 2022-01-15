#pragma once

#include "reactive/widget/widget.h"

#include "reactive/reactivevisibility.h"

#include "reactive/signal/signal.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidget button(AnySignal<std::string> label,
            AnySignal<std::function<void()>> onClick);

    REACTIVE_EXPORT AnyWidget button(std::string label,
            AnySignal<std::function<void()>> onClick);
} // namespace reactive::widget

