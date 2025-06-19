#pragma once

#include "widget.h"

#include "reactive/reactivevisibility.h"

#include <bq/signal/signal.h>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidget button(bq::signal::AnySignal<std::string> label,
            bq::signal::AnySignal<std::function<void()>> onClick);

    REACTIVE_EXPORT AnyWidget button(std::string label,
            bq::signal::AnySignal<std::function<void()>> onClick);
} // namespace reactive::widget

