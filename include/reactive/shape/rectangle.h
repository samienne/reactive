#pragma once

#include "shape.h"

#include "reactive/reactivevisibility.h"

#include "reactive/signal/signal.h"

namespace reactive::shape
{
    REACTIVE_EXPORT AnyShape rectangle();
    REACTIVE_EXPORT AnyShape rectangle(AnySignal<float> cornerRadius);
} // namespace reactive::shape

