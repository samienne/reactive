#pragma once

#include "shape.h"

#include "bqui/bquivisibility.h"

#include <bq/signal/signal.h>

namespace bqui::shape
{
    BQUI_EXPORT AnyShape rectangle();
    BQUI_EXPORT AnyShape rectangle(bq::signal::AnySignal<float> cornerRadius);
}

