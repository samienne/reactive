#pragma once

#include "widget.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidget bin(AnyWidget f,
            bq::signal::AnySignal<avg::Vector2f> contentSize);
} // reactive::widget

