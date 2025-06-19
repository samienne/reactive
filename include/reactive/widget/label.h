#pragma once

#include "widget.h"

#include "reactive/reactivevisibility.h"

#include <bq/signal/signal.h>

#include <string>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidget label(bq::signal::AnySignal<std::string> text);
    REACTIVE_EXPORT AnyWidget label(std::string const& text);
} // reactive::widget

