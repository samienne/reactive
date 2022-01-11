#pragma once

#include "builder.h"

#include "reactive/reactivevisibility.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyBuilder hScrollBar(
            signal::InputHandle<float> handle,
            AnySignal<float> amount,
            AnySignal<float> handleSize);

    REACTIVE_EXPORT AnyBuilder vScrollBar(
            signal::InputHandle<float> handle,
            AnySignal<float> amount,
            AnySignal<float> handleSize);
} // namespace reactive::widget

