#pragma once

#include "widget.h"

#include "reactive/reactivevisibility.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidget hScrollBar(
            bq::signal::InputHandle<float> handle,
            bq::signal::AnySignal<float> amount,
            bq::signal::AnySignal<float> handleSize);

    REACTIVE_EXPORT AnyWidget vScrollBar(
            bq::signal::InputHandle<float> handle,
            bq::signal::AnySignal<float> amount,
            bq::signal::AnySignal<float> handleSize);
} // namespace reactive::widget

