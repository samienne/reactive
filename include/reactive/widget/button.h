#pragma once

#include "reactive/widget/builder.h"

#include "reactive/reactivevisibility.h"

#include "reactive/signal/signal.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyBuilder button(AnySignal<std::string> label,
            AnySignal<std::function<void()>> onClick);

    REACTIVE_EXPORT AnyBuilder button(std::string label,
            AnySignal<std::function<void()>> onClick);
} // namespace reactive::widget

