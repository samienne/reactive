#pragma once

#include "widget.h"

#include "reactive/reactivevisibility.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidget hScrollBar(
            signal::InputHandle<float> handle,
            signal::AnySignal<float> amount,
            signal::AnySignal<float> handleSize);

    REACTIVE_EXPORT AnyWidget vScrollBar(
            signal::InputHandle<float> handle,
            signal::AnySignal<float> amount,
            signal::AnySignal<float> handleSize);
} // namespace reactive::widget

