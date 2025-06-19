#pragma once

#include "shape.h"

#include "reactive/reactivevisibility.h"

#include <bq/signal/signal.h>

namespace reactive::shape
{
    REACTIVE_EXPORT AnyShape rectangle();
    REACTIVE_EXPORT AnyShape rectangle(signal::AnySignal<float> cornerRadius);
} // namespace reactive::shape

