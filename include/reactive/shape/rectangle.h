#pragma once

#include "shape.h"

#include "reactive/reactivevisibility.h"

#include "reactive/signal2/signal.h"

namespace reactive::shape
{
    REACTIVE_EXPORT AnyShape rectangle();
    REACTIVE_EXPORT AnyShape rectangle(signal2::AnySignal<float> cornerRadius);
} // namespace reactive::shape

