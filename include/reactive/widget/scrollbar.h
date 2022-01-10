#pragma once

#include "builder.h"

#include "reactive/reactivevisibility.h"

namespace reactive::widget
{
    REACTIVE_EXPORT Builder hScrollBar(
            signal::InputHandle<float> handle,
            AnySignal<float> amount,
            AnySignal<float> handleSize);

    REACTIVE_EXPORT Builder vScrollBar(
            signal::InputHandle<float> handle,
            AnySignal<float> amount,
            AnySignal<float> handleSize);
} // namespace reactive::widget

