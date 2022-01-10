#pragma once

#include "reactive/widget/builder.h"

#include "reactive/reactivevisibility.h"

#include "reactive/signal/signal.h"

namespace reactive::widget
{
    REACTIVE_EXPORT Builder button(AnySignal<std::string> label,
            AnySignal<std::function<void()>> onClick);

    REACTIVE_EXPORT Builder button(std::string label,
            AnySignal<std::function<void()>> onClick);
} // namespace reactive::widget

