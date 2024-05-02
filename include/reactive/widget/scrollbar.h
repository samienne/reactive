#pragma once

#include "widget.h"

#include "reactive/reactivevisibility.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidget hScrollBar(
            signal2::InputHandle<float> handle,
            signal2::AnySignal<float> amount,
            signal2::AnySignal<float> handleSize);

    REACTIVE_EXPORT AnyWidget vScrollBar(
            signal2::InputHandle<float> handle,
            signal2::AnySignal<float> amount,
            signal2::AnySignal<float> handleSize);
} // namespace reactive::widget

